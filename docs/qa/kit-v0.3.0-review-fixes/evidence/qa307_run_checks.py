"""Bounded, offline unit checks; preserve every named run separately."""
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

BASE = Path('/workspace/qa-review-v030')
KIT = BASE / 'fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0'
CHECKS = {
    'kit-original114': (KIT, ['-B', '-m', 'unittest', 'discover', '-s', 'tools/qa/tests', '-p', 'test_[cde]*.py', '-v']),
    'kit-all': (KIT, ['-B', '-m', 'unittest', 'discover', '-s', 'tools/qa/tests', '-p', 'test_*.py', '-v']),
    'kit-archive-all': (BASE / 'fixes/archive-verification/Floorp_QA_Implementation_Kit_v0.3.0', ['-B', '-m', 'unittest', 'discover', '-s', 'tools/qa/tests', '-p', 'test_*.py', '-v']),
    'runtime-all': (BASE / 'repos/Floorp-Runtime', ['-B', '-m', 'unittest', 'discover', '-s', '.github/workflows/scripts', '-p', 'test_*.py', '-v']),
    'floorp-contracts': (BASE / 'repos/Floorp', ['-B', '-m', 'unittest', 'discover', '-s', 'tools/qa3', '-p', 'test_*.py', '-v']),
    'floorp-source106': (BASE / 'repos/Floorp', ['-B', 'tools/qa3/source.py', 'run', str(BASE / 'evidence/qa307-floorp-source106-result'), '--expected-head', '718e688fbd7ad88a776534e7778ad8e4a7f67364']),
}


def limits(source_v8=False):
    # Runtime's unchanged owned-process tests install a 28-GiB child AS limit.
    # Allow that hard limit while retaining a 2-GiB soft limit for unittest.
    # The enclosing worker cgroup still bounds physical memory independently.
    if not source_v8:
        resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 32 * 1024**3))
    # Deno's pointer cages reserve virtual address space beyond 32 GiB;
    # retain the worker's 16-GiB physical cgroup for the fixed source plan.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    # The native I/O fixture installs a 50-GiB hard FSIZE limit, then enforces
    # its tiny synthetic streaming budgets in run_owned. Do not block prlimit.
    resource.setrlimit(resource.RLIMIT_FSIZE, (32 * 1024**2, 64 * 1024**3))


def main():
    name, revision = sys.argv[1:]
    cwd, arguments = CHECKS[name]
    output = BASE / 'evidence' / f'qa307-{name}-{revision}.log'
    receipt = output.with_suffix('.json')
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    clock = time.monotonic()
    timed_out = False
    env = {'PATH': os.environ['PATH'], 'LANG': 'C.UTF-8', 'TMPDIR': '/tmp',
           'PYTHONDONTWRITEBYTECODE': '1', 'GIT_CONFIG_NOSYSTEM': '1',
           'GIT_CONFIG_GLOBAL': '/dev/null', 'GIT_TERMINAL_PROMPT': '0'}
    if name == 'floorp-source106':
        env['PATH'] = str(BASE / 'tooling/node22/bin') + ':' + str(BASE / 'tooling') + ':' + env['PATH']
        env['DENO_DIR'] = str(BASE / 'tooling/deno-cache')
        arguments = list(arguments)
        arguments[3] = str(BASE / 'evidence' / f'qa307-floorp-source106-{revision}')
    command = [sys.executable, *arguments]
    with output.open('xb') as stream:
        child = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream,
                                 stderr=subprocess.STDOUT, start_new_session=True,
                                 preexec_fn=lambda: limits(name == 'floorp-source106'))
        try:
            exit_code = child.wait(timeout=45)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(child.pid, signal.SIGKILL)
            exit_code = child.wait()
    raw = output.read_bytes()
    result = {'check': name, 'revision': revision, 'startedAt': started,
              'command': command, 'cwd': str(cwd), 'exitCode': exit_code,
              'timedOut': timed_out, 'seconds': round(time.monotonic() - clock, 3),
              'log': output.name, 'logSha256': hashlib.sha256(raw).hexdigest(),
              'networkRequired': False, 'browserRequired': False,
              'outerSoftASBytes': None if name == 'floorp-source106' else 2 * 1024**3,
              'outerHardASBytes': None if name == 'floorp-source106' else 32 * 1024**3,
              'workerPhysicalMemoryMaxBytes': int(Path('/sys/fs/cgroup/memory.max').read_text()),
              'outerSoftFSIZEBytes': 32 * 1024**2, 'outerHardFSIZEBytes': 64 * 1024**3}
    with receipt.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps(result))
    print(raw.decode('utf-8', errors='replace')[-4000:])
    return int(exit_code != 0 or timed_out)


if __name__ == '__main__':
    raise SystemExit(main())
