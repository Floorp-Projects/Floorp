"""Retain each bounded local check and its exact source head."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import time

ROOT = Path("/workspace/qa-review-v030")
EVIDENCE = ROOT / "fix-qa309/evidence"
name, label = sys.argv[1:]
floorp = ROOT / "repos/Floorp"
runtime = ROOT / "repos/Floorp-Runtime"
env = {
    "PATH": os.environ["PATH"],
    "LANG": "C.UTF-8",
    "TMPDIR": "/tmp",
    "PYTHONDONTWRITEBYTECODE": "1",
    "GIT_CONFIG_NOSYSTEM": "1",
    "GIT_CONFIG_GLOBAL": "/dev/null",
    "GIT_TERMINAL_PROMPT": "0",
}
source_v8 = name == "floorp-source106"
cwd = floorp if name.startswith("floorp-") else runtime
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=cwd, env=env, text=True).strip()
if name == "floorp-source106":
    env["PATH"] = f"{ROOT}/tooling/node22/bin:{ROOT}/tooling:" + env["PATH"]
    env["DENO_DIR"] = str(ROOT / "tooling/deno-cache")
    args = ["-B", "tools/qa3/source.py", "run", str(EVIDENCE / f"{name}-{label}"), "--expected-head", head]
elif name == "floorp-contracts":
    args = ["-B", "-m", "unittest", "discover", "-s", "tools/qa3", "-p", "test_*.py", "-v"]
elif name == "runtime-all":
    args = ["-B", "-m", "unittest", "discover", "-s", ".github/workflows/scripts", "-p", "test_*.py", "-v"]
else:
    raise ValueError("unregistered local check")


def limits():
    if not source_v8:
        resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 32 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_FSIZE, (32 * 1024**2, 64 * 1024**3))


log = EVIDENCE / f"{name}-{label}.log"
started = datetime.datetime.now(datetime.timezone.utc).isoformat()
clock = time.monotonic()
timed_out = False
command = [sys.executable, *args]
with log.open("xb") as stream:
    child = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True, preexec_fn=limits)
    try:
        code = child.wait(timeout=150)
    except subprocess.TimeoutExpired:
        timed_out = True
        os.killpg(child.pid, signal.SIGKILL)
        code = child.wait(timeout=5)
result = {
    "check": name,
    "label": label,
    "sourceHead": head,
    "startedAt": started,
    "command": command,
    "cwd": str(cwd),
    "exitCode": code,
    "timedOut": timed_out,
    "seconds": round(time.monotonic() - clock, 3),
    "log": log.name,
    "logSha256": hashlib.sha256(log.read_bytes()).hexdigest(),
    "credentialsInherited": False,
    "browserRequired": False,
    "workerPhysicalMemoryMaxBytes": int(Path("/sys/fs/cgroup/memory.max").read_text()),
    "outerSoftASBytes": None if source_v8 else 2 * 1024**3,
    "outerHardASBytes": None if source_v8 else 32 * 1024**3,
    "outerSoftFSIZEBytes": 32 * 1024**2,
    "outerHardFSIZEBytes": 64 * 1024**3,
    "wallTimeoutSeconds": 150,
}
with (EVIDENCE / f"{name}-{label}.json").open("x") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(json.dumps(result))
raise SystemExit(int(code != 0 or timed_out))
