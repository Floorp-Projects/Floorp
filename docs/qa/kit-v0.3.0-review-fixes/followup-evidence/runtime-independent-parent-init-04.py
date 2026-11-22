import ctypes, errno, hashlib, importlib.util, json, os, signal, sys, time
from pathlib import Path
from unittest import mock
SCRIPTS=Path("/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts")
sys.path.insert(0,str(SCRIPTS))
import qa3_native_io as io
EXPECTED="8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e"
assert hashlib.sha256(Path(io.__file__).read_bytes()).hexdigest()==EXPECTED
io.MAX_EXPANDED=8*1024**2
mode=sys.argv[1] if len(sys.argv)>1 else "raise"
root=Path("/workspace/qa-review-v030/fix-qa309/evidence/runtime-independent-parent-init-fixtures-04")/mode
root.mkdir(parents=True,exist_ok=False)
caller=os.getpid()
libc=ctypes.CDLL(None)
prior=ctypes.c_int()
assert libc.prctl(37,ctypes.byref(prior),0,0,0)==0
assert libc.prctl(36,1,0,0,0)==0
real_proc=io._proc
real_fork=os.fork
real_pipe=os.pipe
real_block=os.set_blocking
real_close=os.close
real_pidfd=os.pidfd_open
real_kill=os.kill
real_waitid=os.waitid
parent_pidfds=[]
controls=[]
held_signals=[]
waitid_flags=[]
close_faults=[]
supervisors=[]
pipes=[]
faults=[]
marker=root/"command.json"
child=("import json,os,pathlib,signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); "
       "birth=int(pathlib.Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19]); "
       f"pathlib.Path({str(marker)!r}).write_text(json.dumps({{'pid':os.getpid(),'birth':birth}})); "
       "print('own fixture started',flush=True); time.sleep(30)")
def pipe():
    reader,writer=real_pipe()
    if os.getpid()==caller:
        controls.extend((fd,os.fstat(fd)) for fd in (reader,writer))
        stat=os.fstat(writer)
        pipes.append((reader,writer,stat.st_dev,stat.st_ino))
    return reader,writer
def fork():
    pid=real_fork()
    if pid>0 and os.getpid()==caller:
        supervisors.append((pid,real_proc(pid)))
        deadline=time.monotonic()+2
        while not marker.exists() and time.monotonic()<deadline: time.sleep(.005)
        assert marker.exists(), "independent own command did not start"
    return pid
def proc(pid):
    if os.getpid()==caller and not faults:
        faults.append("parent initial snapshot")
        if mode in {"raise", "close-reader", "close-pidfd"}:
            raise OSError(errno.EIO,"independent parent initial snapshot failure")
        return None
    return real_proc(pid)
def blocking(fd,flag):
    if mode in {"none", "fallback"} and os.getpid()==caller:
        faults.append("parent reader initialization")
        raise OSError(errno.EIO,"independent parent reader setup failure after missing birth")
    return real_block(fd,flag)
def pidfd_open(pid, *args):
    fd=real_pidfd(pid,*args)
    if os.getpid()==caller:
        parent_pidfds.append(fd)
        controls.append((fd,os.fstat(fd)))
    return fd

def close(fd):
    selected=(pipes[0][0] if mode=="close-reader" and pipes else parent_pidfds[0] if mode=="close-pidfd" and parent_pidfds else None)
    if os.getpid()==caller and fd==selected and not close_faults:
        real_close(fd)
        close_faults.append(fd)
        raise OSError(errno.EIO,"secondary independent control close failure after closing")
    return real_close(fd)

def kill(pid,sig):
    if os.getpid()==caller and any(pid==owned for owned,_ in supervisors):
        held_signals.append(int(sig))
    return real_kill(pid,sig)

def waitid(which,pid,flags):
    if os.getpid()==caller and any(pid==owned for owned,_ in supervisors):
        waitid_flags.append(flags)
    return real_waitid(which,pid,flags)

def exact_signal(pid,birth,sig):
    info=real_proc(pid)
    if not info or info[2]!=birth: return False
    fd=os.pidfd_open(pid)
    try:
        current=real_proc(pid)
        if current and current[2]==birth:
            signal.pidfd_send_signal(fd,sig)
            return True
    finally: real_close(fd)
    return False
error=None; observed=None; cleanup=[]
started=time.monotonic()
try:
    with mock.patch.object(io.os,"pipe",side_effect=pipe),mock.patch.object(io.os,"fork",side_effect=fork),mock.patch.object(io,"_proc",side_effect=proc),mock.patch.object(io.os,"set_blocking",side_effect=blocking),mock.patch.object(io.os,"close",side_effect=close),mock.patch.object(io.os,"pidfd_open",new=None if mode=="fallback" else pidfd_open),mock.patch.object(io.os,"kill",side_effect=kill),mock.patch.object(io.os,"waitid",side_effect=waitid):
        try:
            io.run_owned([sys.executable,"-B","-c",child],root,io.child_environment(root/"state"),root/"logs","parent-init",20,memory=128*1024**2)
        except BaseException as exc:
            error={"type":type(exc).__name__,"errno":getattr(exc,"errno",None),"message":str(exc),"notes":getattr(exc,"__notes__",[])}
        else:
            error={"unexpected":"zero-error return"}
    assert marker.exists(),"fixture command did not initialize"
    child_id=json.loads(marker.read_text())
    pid,sup=supervisors[0]
    supervisor_info=real_proc(pid)
    command_info=real_proc(child_id["pid"])
    writer_open=False
    for _,fd,dev,inode in pipes:
        try: stat=os.fstat(fd)
        except OSError: continue
        if (stat.st_dev,stat.st_ino)==(dev,inode): writer_open=True
    observed={"case":mode,"error":error,"durationSeconds":round(time.monotonic()-started,3),"faults":faults,"supervisorAliveAfterParentReturned":bool(supervisor_info and sup and supervisor_info[2]==sup[2] and supervisor_info[1]!="Z"),"commandAliveAfterParentReturned":bool(command_info and command_info[2]==child_id["birth"] and command_info[1]!="Z"),"processReceiptExistsBeforeFixtureCleanup":(root/"logs/parent-init.process.json").exists(),"parentControlWriterStillOpen":writer_open,"sourceSHA256":EXPECTED,"nativeOrBrowserExecuted":False,"fixtureRegularFileLimitBytes":8*1024**2}
    open_before_cleanup=[]
    for fd,stat in controls:
        try: current=os.fstat(fd)
        except OSError: continue
        if (current.st_dev,current.st_ino)==(stat.st_dev,stat.st_ino): open_before_cleanup.append(fd)
    observed["parentOwnOpenFDsBeforeFixtureCleanup"]=open_before_cleanup
finally:
    for pid,info in supervisors:
        if not info: continue
        exact_signal(pid,info[2],signal.SIGTERM)
        deadline=time.monotonic()+8
        done=False
        while time.monotonic()<deadline:
            try: waited,status=os.waitpid(pid,os.WNOHANG)
            except ChildProcessError: done=True; break
            if waited: done=True; break
            time.sleep(.02)
        if not done:
            exact_signal(pid,info[2],signal.SIGKILL)
            os.waitpid(pid,0)
        cleanup.append({"supervisor":pid,"reaped":True})
    if marker.exists():
        owned=json.loads(marker.read_text())
        info=real_proc(owned["pid"])
        if info and info[2]==owned["birth"]:
            if info[1]!="Z": exact_signal(owned["pid"],owned["birth"],signal.SIGKILL)
            try: os.waitpid(owned["pid"],0)
            except ChildProcessError: pass
        cleanup.append({"command":owned["pid"],"absentAfterCleanup":real_proc(owned["pid"]) is None})
    for _,fd,dev,inode in pipes:
        try: stat=os.fstat(fd)
        except OSError: continue
        if (stat.st_dev,stat.st_ino)==(dev,inode): real_close(fd)
    for fd,stat in controls:
        try: current=os.fstat(fd)
        except OSError: continue
        if (current.st_dev,current.st_ino)==(stat.st_dev,stat.st_ino): real_close(fd)
    assert libc.prctl(36,prior.value,0,0,0)==0
assert observed is not None
observed["fixtureCleanup"]=cleanup
observed["callerSubreaperRestored"]=True
assert error["type"]=="OSError" and error["errno"]==errno.EIO, observed
assert not observed["supervisorAliveAfterParentReturned"], observed
assert not observed["commandAliveAfterParentReturned"], observed
assert not observed["parentControlWriterStillOpen"], observed
assert observed["processReceiptExistsBeforeFixtureCleanup"], observed
receipt=json.loads((root/"logs/parent-init.process.json").read_text())
assert receipt["ownedCleanupComplete"] is True and receipt["cancelled"] is True, receipt
observed["processReceipt"]=receipt
observed["controlCloseFaults"]=close_faults
observed["heldChildSignals"]=held_signals
observed["waitidFlags"]=waitid_flags
open_controls=[]
for fd,stat in controls:
    try: current=os.fstat(fd)
    except OSError: continue
    if (current.st_dev,current.st_ino)==(stat.st_dev,stat.st_ino): open_controls.append(fd)
assert not open_controls, open_controls
assert not observed["parentOwnOpenFDsBeforeFixtureCleanup"], observed
observed["allOwnParentFDsClosed"]=True
if mode in {"close-reader","close-pidfd"}:
    assert close_faults and error["notes"], observed
    notes=json.loads(error["notes"][0])["ownedSupervisorCleanupErrors"]
    key="CLOSE_READER" if mode=="close-reader" else "CLOSE_PIDFD"
    assert notes[key]["errno"]==errno.EIO, notes
if mode=="fallback":
    assert held_signals and waitid_flags, observed
    assert all(flags & os.WNOWAIT for flags in waitid_flags), observed
output=Path("/workspace/qa-review-v030/fix-qa309/evidence")/f"runtime-independent-parent-init-{mode}-04.json"
output.write_text(json.dumps(observed,indent=2)+"\n")
print(json.dumps(observed))

