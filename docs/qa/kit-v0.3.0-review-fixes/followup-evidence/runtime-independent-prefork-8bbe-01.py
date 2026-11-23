import errno, hashlib, json, os, signal, sys, threading
from pathlib import Path
from unittest import mock
scripts=Path("/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts")
sys.path.insert(0,str(scripts))
import qa3_native_io as io
expected="8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e"
assert hashlib.sha256(Path(io.__file__).read_bytes()).hexdigest()==expected
results=[]
original_sigchld=signal.getsignal(signal.SIGCHLD)
def guard(name):
    with mock.patch.object(io.os,"pipe",side_effect=AssertionError("guard allocated control pipe")) as pipe, mock.patch.object(io.os,"fork",side_effect=AssertionError("guard forked")) as fork:
        try: io._supervise(lambda cancelled: {},1,None)
        except ValueError as error:
            results.append({"case":name,"error":str(error),"pipeCalls":pipe.call_count,"forkCalls":fork.call_count})
        else: raise AssertionError("guard accepted unsupported caller state")
    assert not pipe.call_count and not fork.call_count
try:
    signal.signal(signal.SIGCHLD,signal.SIG_IGN)
    guard("actual ignored SIGCHLD")
finally: signal.signal(signal.SIGCHLD,original_sigchld)
started=threading.Event(); release=threading.Event()
def worker(): started.set(); release.wait(2)
thread=threading.Thread(target=worker)
thread.start()
try:
    assert started.wait(1)
    tasks=len(list(Path("/proc/self/task").iterdir()))
    assert tasks>=2
    guard("actual additional OS thread")
    results[-1]["actualOSTasks"]=tasks
finally:
    release.set(); thread.join(timeout=2)
assert not thread.is_alive()
assert signal.getsignal(signal.SIGCHLD)==original_sigchld
real_pipe=os.pipe; real_close=os.close
controls=[]; close_errors=[]
initial=OSError(errno.EAGAIN,"independent own fork initialization failure")
def pipe():
    pair=real_pipe(); controls.extend((fd,os.fstat(fd)) for fd in pair); return pair
def close(fd):
    if controls and fd==controls[0][0] and not close_errors:
        real_close(fd); close_errors.append(fd)
        raise OSError(errno.EIO,"independent fork cleanup close fault after closing")
    return real_close(fd)
try:
    with mock.patch.object(io.os,"pipe",side_effect=pipe),mock.patch.object(io.os,"fork",side_effect=initial),mock.patch.object(io.os,"close",side_effect=close):
        try: io._supervise(lambda cancelled: {},1,None)
        except OSError as error: assert error is initial
        else: raise AssertionError("fork failure accepted")
    still_open=[]
    for fd,stat in controls:
        try: current=os.fstat(fd)
        except OSError: continue
        if (current.st_dev,current.st_ino)==(stat.st_dev,stat.st_ino): still_open.append(fd)
    assert not still_open,still_open
    results.append({"case":"fork EAGAIN with close-after-close EIO","initialErrorPreserved":True,"allControlFDsClosedBeforeFixtureRecovery":True,"closeFaultCount":len(close_errors)})
finally:
    for fd,stat in controls:
        try: current=os.fstat(fd)
        except OSError: continue
        if (current.st_dev,current.st_ino)==(stat.st_dev,stat.st_ino): real_close(fd)
value={"sourceSHA256":expected,"cases":results,"SIGCHLDRestored":True,"nativeOrBrowserExecuted":False}
output=Path("/workspace/qa-review-v030/fix-qa309/evidence/runtime-independent-prefork-8bbe-01.json")
with output.open("x") as stream: json.dump(value,stream,indent=2); stream.write("\n")
print(json.dumps(value))
