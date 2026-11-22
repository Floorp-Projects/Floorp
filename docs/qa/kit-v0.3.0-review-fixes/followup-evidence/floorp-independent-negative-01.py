import ctypes
import errno
import hashlib
import importlib.util
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from unittest import mock

SOURCE = Path("/workspace/qa-review-v030/repos/Floorp/tools/qa3/source.py")
ROOT = Path("/workspace/qa-review-v030/fix-qa309/evidence/floorp-independent-fixtures-01")
ROOT.mkdir(exist_ok=False) if not ROOT.exists() else None
EXPECT = "99698eec43bc357db800a3400969595224f09ed3ffc77f80ec139f8f4a6c2115"
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECT
spec = importlib.util.spec_from_file_location("independent_source", SOURCE)
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
results = {"sourceSHA256": EXPECT, "nativeOrBrowserExecuted": False, "checks": []}

def state():
    value = ctypes.c_int()
    assert ctypes.CDLL(None).prctl(37, ctypes.byref(value), 0, 0, 0) == 0
    return value.value

def fault_helper():
    initial = state()
    read_fd, write_fd = os.pipe()
    marker = ROOT / "pidfd-fault-child.json"
    child = ("import json,os,pathlib,signal,time; "
             "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
             "birth=int(pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]); "
             f"pathlib.Path({str(marker)!r}).write_text(json.dumps({{'pid':os.getpid(),'birth':birth}})); "
             "time.sleep(20)")
    original_spawn = subprocess.Popen
    original_pidfd = os.pidfd_open
    spawned = []
    failures = []
    def spawn(*args, **kwargs):
        process = original_spawn(*args, **kwargs)
        spawned.append((process, source._process_info(process.pid)))
        return process
    def transient(pid, *args, **kwargs):
        if not failures:
            failures.append(pid)
            raise OSError(errno.EMFILE, "synthetic transient pidfd resource failure")
        return original_pidfd(pid, *args, **kwargs)
    before_return = None
    statuses = None
    cleanup = []
    try:
        with mock.patch.object(source.subprocess, "Popen", side_effect=spawn), mock.patch.object(source.os, "pidfd_open", side_effect=transient):
            source._execute_owned(write_fd, 0.3, [sys.executable, "-c", child])
        write_fd = None
        statuses = json.loads(os.read(read_fd, 4096))
        assert marker.exists(), "fixture child did not initialize"
        owned = json.loads(marker.read_text())
        info = source._process_info(owned["pid"])
        before_return = bool(info and info[2] == owned["birth"] and info[1] != "Z")
    finally:
        if write_fd is not None:
            try: os.close(write_fd)
            except OSError: pass
        os.close(read_fd)
        for process, birth_info in spawned:
            info = source._process_info(process.pid)
            if info and birth_info and info[2] == birth_info[2] and info[1] != "Z":
                fd = original_pidfd(process.pid)
                try:
                    current = source._process_info(process.pid)
                    if current and current[2] == birth_info[2]:
                        signal.pidfd_send_signal(fd, signal.SIGKILL)
                finally:
                    os.close(fd)
            process.wait(timeout=3)
            cleanup.append({"pid":process.pid, "reaped":True})
        assert ctypes.CDLL(None).prctl(36, initial, 0, 0, 0) == 0
    observation = {"case":"transient-pidfd-error-in-cleanup", "faults":len(failures), "status":statuses,
                   "childAliveAfterHelperReturned":before_return, "fixtureCleanup":cleanup,
                   "faultInjection":"one EMFILE at real pidfd_open call; subsequent calls would succeed",
                   "subreaperRestored":state()==initial}
    (ROOT / "pidfd-fault-observation.json").write_text(json.dumps(observation, indent=2)+"\n")
    print(json.dumps(observation))
    return

if len(sys.argv)>1 and sys.argv[1]=="fault":
    fault_helper()
    raise SystemExit(0)

document = '<testsuite tests="1"><testcase classname="tools/qa3/test.xml" name="日本語"/></testsuite>'
unsafe = '<!DOCTYPE testsuite [<!ENTITY item "safe">]><testsuite tests="1"><testcase classname="tools/qa3/test.xml" name="&item;"/></testsuite>'
xml_results = []
for encoding in ("utf-16", "utf-16-le", "utf-16-be", "utf-32", "utf-32-le", "utf-32-be"):
    for kind, text in (("ordinary", document), ("DTD", unsafe)):
        path=ROOT/f"xml-{encoding}-{kind}.xml"
        path.write_bytes(text.encode(encoding))
        with mock.patch.object(source.ET, "fromstring", wraps=source.ET.fromstring) as parser:
            try: source.read_junit(path, ROOT)
            except ValueError as error: rejected=True; detail=str(error)
            else: rejected=False; detail=None
            assert rejected and parser.call_count==0
            xml_results.append({"encoding":encoding,"kind":kind,"rejected":True,"parserCalls":parser.call_count,"error":detail})
for declaration in ("UTF-16", "UTF-32", "ISO-8859-1"):
    path=ROOT/f"xml-declared-{declaration}.xml"
    path.write_bytes((f'<?xml version="1.0" encoding="{declaration}"?>'+document).encode("utf-8"))
    with mock.patch.object(source.ET, "fromstring", wraps=source.ET.fromstring) as parser:
        try: source.read_junit(path, ROOT)
        except ValueError: pass
        else: raise AssertionError("different XML encoding accepted")
        assert parser.call_count==0
        xml_results.append({"encoding":declaration,"kind":"UTF-8 bytes with different declaration","rejected":True,"parserCalls":0})
for encoding in ("utf-8", "utf-8-sig"):
    path=ROOT/f"xml-safe-{encoding}.xml"
    path.write_bytes(('<?xml version="1.0" encoding="UTF-8"?>'+document).encode(encoding))
    assert source.read_junit(path, ROOT)[0]["name"]=="日本語"
    xml_results.append({"encoding":encoding,"kind":"valid","accepted":True})
    path=ROOT/f"xml-dtd-{encoding}.xml"
    path.write_bytes(unsafe.encode(encoding))
    with mock.patch.object(source.ET, "fromstring", wraps=source.ET.fromstring) as parser:
        try: source.read_junit(path, ROOT)
        except ValueError: pass
        else: raise AssertionError("UTF-8 DTD accepted")
        assert parser.call_count==0
        xml_results.append({"encoding":encoding,"kind":"DTD","rejected":True,"parserCalls":0})
results["xml"] = xml_results

before = state()
command = """import json,os
from pathlib import Path
open_fds={}
for entry in Path('/proc/self/fd').iterdir():
    try: open_fds[entry.name]=os.readlink(entry)
    except FileNotFoundError: pass
print(json.dumps({'fds':open_fds,'token':os.environ.get('GITHUB_TOKEN'),'nodeOptions':os.environ.get('NODE_OPTIONS'),'pythonPath':os.environ.get('PYTHONPATH')}))
"""
with mock.patch.dict(os.environ, {"GITHUB_TOKEN":"independent-fixture-token","NODE_OPTIONS":"independent-invalid","PYTHONPATH":"independent-invalid"}):
    assert source.execute([sys.executable, "-c", command], ROOT/"fd-env.log", ROOT, 2)==0
observed = json.loads((ROOT/"fd-env.log").read_text())
assert set(observed["fds"])=={"0","1","2"} and observed["token"] is None and observed["nodeOptions"] is None and observed["pythonPath"] is None
assert state()==before
results["controlFDAndEnvironment"] = {"fds":observed["fds"],"fixtureTokensAbsent":True,"parentSubreaperUnchanged":True}

class Completed:
    returncode=0
    def wait(self, **kwargs): return 0
payloads = [
    ('valid','{"error":null,"exitCode":0}',True),
    ('bool','{"error":null,"exitCode":false}',False),
    ('float','{"error":null,"exitCode":0.0}',False),
    ('string','{"error":null,"exitCode":"0"}',False),
    ('missing-exit','{"error":null}',False),
    ('missing-error','{"exitCode":0}',False),
    ('null','null',False),
    ('malformed','{',False),
    ('reported-error','{"error":"cleanup failure","exitCode":0}',False),
]
protocol=[]
for name, payload, expected in payloads:
    def fake_spawn(argv, **kwargs):
        os.write(kwargs["pass_fds"][0],payload.encode())
        return Completed()
    with mock.patch.object(source.subprocess,"Popen",side_effect=fake_spawn):
        try: value=source.execute(["/fixture-only"], ROOT/f"protocol-{name}.log", ROOT, 1)
        except Exception as error: accepted=False; outcome=type(error).__name__
        else: accepted=value==0; outcome=value
    assert accepted==expected
    protocol.append({"case":name,"acceptedAsZero":accepted,"outcome":outcome,"fixtureOnly":True})
results["protocol"]=protocol

fake=mock.Mock(pid=43210)
race_results=[]
for name, new_birth, open_error, signal_error in (("reused-before-recheck",200,None,None),("exited-before-open",100,ProcessLookupError(),None),("exited-before-signal",100,None,ProcessLookupError())):
    snapshots=[{43210:(os.getpid(),"S",100)}]
    def snapshot(_): return snapshots.pop(0) if snapshots else {}
    sent=[]
    def signal_fd(fd,sig):
        sent.append((fd,sig))
        if signal_error: raise signal_error
    with mock.patch.object(source,"_owned_descendants",side_effect=snapshot),mock.patch.object(source,"_process_info",return_value=(os.getpid(),"S",new_birth)),mock.patch.object(source.os,"pidfd_open",side_effect=open_error or (lambda pid:77)),mock.patch.object(source.signal,"pidfd_send_signal",side_effect=signal_fd),mock.patch.object(source.os,"close"),mock.patch.object(source.os,"waitpid",return_value=(0,0)):
        source._cleanup_owned(fake,{43210:100})
    if name=="reused-before-recheck": assert sent==[]
    race_results.append({"case":name,"signalCalls":len(sent),"fixtureOnly":True,"wrongBirthSignalled":False})
results["pidfdRaceModels"]=race_results

child = subprocess.run([sys.executable,"-B",__file__,"fault"],capture_output=True,text=True,timeout=12)
(ROOT/"fault-helper.stdout").write_text(child.stdout)
(ROOT/"fault-helper.stderr").write_text(child.stderr)
assert child.returncode==0,(child.returncode,child.stderr)
results["transientCleanupFault"]=json.loads((ROOT/"pidfd-fault-observation.json").read_text())
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECT
output=Path("/workspace/qa-review-v030/fix-qa309/evidence/floorp-independent-negative-01.json")
output.write_text(json.dumps(results,indent=2)+"\n")
print(json.dumps({"xmlCases":len(xml_results),"protocolCases":len(protocol),"raceModels":len(race_results),"cleanupFaultChildAlive":results["transientCleanupFault"]["childAliveAfterHelperReturned"],"sourceBytesUnchanged":True,"nativeOrBrowserExecuted":False}))

