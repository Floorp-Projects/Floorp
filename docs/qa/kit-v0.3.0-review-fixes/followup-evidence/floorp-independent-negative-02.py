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
ROOT = Path("/workspace/qa-review-v030/fix-qa309/evidence/floorp-independent-fixtures-02")
ROOT.mkdir(exist_ok=False) if not ROOT.exists() else None
EXPECT = "5cb0803d54c47197e0597cd93de436c60cb2f667340f5bcff55e6f0326b30d32"
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECT
spec = importlib.util.spec_from_file_location("independent_source", SOURCE)
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
results = {"sourceSHA256": EXPECT, "nativeOrBrowserExecuted": False, "checks": []}

def state():
    value = ctypes.c_int()
    assert ctypes.CDLL(None).prctl(37, ctypes.byref(value), 0, 0, 0) == 0
    return value.value

def fault_helper(mode):
    initial = state()
    read_fd, write_fd = os.pipe()
    marker = ROOT / f"{mode}-fault-child.json"
    child = ("import json,os,pathlib,signal,time; "
             "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
             "birth=int(pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]); "
             f"pathlib.Path({str(marker)!r}).write_text(json.dumps({{'pid':os.getpid(),'birth':birth}})); "
             "time.sleep(20)")
    original_spawn = subprocess.Popen
    original_pidfd = os.pidfd_open
    original_close = os.close
    original_cleanup = source._cleanup_owned
    original_descendants = source._owned_descendants
    spawned = []
    failures = []
    opened = set()
    in_cleanup = False
    def spawn(*args, **kwargs):
        process = original_spawn(*args, **kwargs)
        spawned.append((process, source._process_info(process.pid)))
        return process
    def transient(pid, *args, **kwargs):
        if mode == "pidfd" and not failures:
            failures.append(pid)
            raise OSError(errno.EMFILE, "synthetic transient pidfd resource failure")
        fd = original_pidfd(pid, *args, **kwargs)
        opened.add(fd)
        return fd
    def close(fd):
        was_pidfd = fd in opened
        original_close(fd)
        opened.discard(fd)
        if mode == "close" and was_pidfd and not failures:
            failures.append(fd)
            raise OSError(errno.EIO, "synthetic pidfd close failure after fd closed")
    def descendants(owned):
        if mode == "snapshot" and in_cleanup and not failures:
            failures.append(True)
            raise OSError(errno.EIO, "synthetic transient process snapshot failure")
        return original_descendants(owned)
    def cleanup(process, owned):
        nonlocal in_cleanup
        in_cleanup = True
        return original_cleanup(process, owned)
    before_return = None
    statuses = None
    fixture_cleanup = []
    try:
        with mock.patch.object(source.subprocess, "Popen", side_effect=spawn), mock.patch.object(source.os, "pidfd_open", side_effect=transient), mock.patch.object(source.os, "close", side_effect=close), mock.patch.object(source, "_cleanup_owned", side_effect=cleanup), mock.patch.object(source, "_owned_descendants", side_effect=descendants):
            source._execute_owned(write_fd, 0.3, [sys.executable, "-c", child])
        write_fd = None
        statuses = json.loads(os.read(read_fd, 4096))
        assert marker.exists(), "fixture child did not initialize"
        owned = json.loads(marker.read_text())
        info = source._process_info(owned["pid"])
        before_return = bool(info and info[2] == owned["birth"])
        assert before_return is False, "helper left a live or zombie owned process"
        assert statuses["ownedCleanupComplete"] is True
        assert len(statuses["cleanupErrors"]) == 1
        assert "timed out" in statuses["error"]
        assert statuses["exitCode"] is None
    finally:
        if write_fd is not None:
            try: original_close(write_fd)
            except OSError: pass
        original_close(read_fd)
        for fd in list(opened):
            try: original_close(fd)
            except OSError: pass
        for process, birth_info in spawned:
            info = source._process_info(process.pid)
            external_kill = False
            if info and birth_info and info[2] == birth_info[2] and info[1] != "Z":
                fd = original_pidfd(process.pid)
                try:
                    current = source._process_info(process.pid)
                    if current and current[2] == birth_info[2]:
                        signal.pidfd_send_signal(fd, signal.SIGKILL)
                        external_kill = True
                finally:
                    original_close(fd)
            process.wait(timeout=3)
            fixture_cleanup.append({"pid":process.pid, "externalKillRequired":external_kill, "fixtureWaitCalled":True})
        assert ctypes.CDLL(None).prctl(36, initial, 0, 0, 0) == 0
    observation = {"case":f"transient-{mode}-error-in-cleanup", "faults":len(failures), "status":statuses,
                   "childExistsAfterHelperReturned":before_return, "fixtureCleanup":fixture_cleanup,
                   "faultInjection":"one fault at the actual helper syscall boundary; later calls succeed",
                   "subreaperRestored":state()==initial}
    (ROOT / f"{mode}-fault-observation.json").write_text(json.dumps(observation, indent=2)+"\n")
    print(json.dumps(observation))

if len(sys.argv)>1 and sys.argv[1]=="fault":
    fault_helper(sys.argv[2])
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
base = {"error":None,"exitCode":0,"cleanupErrors":[],"ownedCleanupComplete":True}
invalid = [
    ("bool-exit",{**base,"exitCode":False}),
    ("float-exit",{**base,"exitCode":0.0}),
    ("string-exit",{**base,"exitCode":"0"}),
    ("bool-cleanup",{**base,"ownedCleanupComplete":1}),
    ("incomplete-cleanup",{**base,"ownedCleanupComplete":False}),
    ("missing-cleanup",{k:v for k,v in base.items() if k!="ownedCleanupComplete"}),
    ("missing-error",{k:v for k,v in base.items() if k!="error"}),
    ("null-cleanup-errors",{**base,"cleanupErrors":None}),
    ("untyped-cleanup-error",{**base,"cleanupErrors":[False]}),
    ("too-many-cleanup-errors",{**base,"cleanupErrors":["x"]*5}),
    ("recovered-cleanup-error",{**base,"cleanupErrors":["recovered EIO"]}),
    ("untyped-error",{**base,"error":False}),
    ("empty-error",{**base,"error":""}),
    ("reported-error",{**base,"error":"cleanup interrupted"}),
    ("extra-field",{**base,"extra":True}),
    ("null",None),
    ("array",[]),
]
payloads = [("valid",json.dumps(base),True),("nonzero",json.dumps({**base,"exitCode":124}),False)]
payloads += [(name,json.dumps(value),False) for name,value in invalid]
payloads += [("malformed","{",False),("oversized","x"*4097,False)]
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

faults=[]
for mode in ("pidfd","close","snapshot"):
    child = subprocess.run([sys.executable,"-B",__file__,"fault",mode],capture_output=True,text=True,timeout=12)
    (ROOT/f"{mode}-helper.stdout").write_text(child.stdout)
    (ROOT/f"{mode}-helper.stderr").write_text(child.stderr)
    assert child.returncode==0,(mode,child.returncode,child.stderr)
    observation=json.loads((ROOT/f"{mode}-fault-observation.json").read_text())
    assert observation["childExistsAfterHelperReturned"] is False
    assert all(not c["externalKillRequired"] for c in observation["fixtureCleanup"])
    faults.append(observation)
results["cleanupFaults"]=faults
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECT
output=Path("/workspace/qa-review-v030/fix-qa309/evidence/floorp-independent-negative-02.json")
output.write_text(json.dumps(results,indent=2)+"\n")
print(json.dumps({"xmlCases":len(xml_results),"protocolCases":len(protocol),"raceModels":len(race_results),"cleanupFaultCases":len(faults),"cleanupFaultsRecovered":all(not f["childExistsAfterHelperReturned"] for f in faults),"sourceBytesUnchanged":True,"nativeOrBrowserExecuted":False}))

