import ast
import hashlib
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

REPO = Path('/workspace/qa-review-v030/repos/Floorp-Runtime')
EVIDENCE = Path('/workspace/qa-review-v030/fix-qa309/evidence')
PYTHON = '/opt/codex/runtimes/codex-primary-runtime/dependencies/python/bin/python3'
RUFF = '/workspace/qa-review-v030/tooling/python-lint/bin/ruff'
FILES = ['.github/workflows/scripts/qa3_mozlog.py', '.github/workflows/scripts/test_qa3_mozlog.py']
COMMANDS = {
    'green-02': [PYTHON, '-B', '-m', 'unittest', 'discover', '-s', '.github/workflows/scripts', '-p', 'test_qa3_mozlog.py', '-v'],
    'ruff-check-green-01': [RUFF, 'check', '--no-cache', *FILES],
    'ruff-format-green-01': [RUFF, 'format', '--check', '--no-cache', *FILES],
    'mach-lint-01': [PYTHON, '-B', str(REPO / 'mach'), 'lint', '--linter', 'ruff', '--linter', 'ruff-format', *FILES],
}

stage = sys.argv[1]
command = COMMANDS[stage]
memory = 536870912 if stage == 'green-02' else 2147483648
cpu = 15 if stage == 'green-02' else 30
wall = 30 if stage == 'green-02' else 60
argv = ['/usr/bin/prlimit', f'--as={memory}', f'--cpu={cpu}', '--fsize=8388608', '--', *command]
env = {
    'PATH': os.environ.get('PATH', '/usr/bin:/bin'),
    'GIT_CONFIG_GLOBAL': '/dev/null',
    'GIT_CONFIG_NOSYSTEM': '1',
    'LANG': 'C.UTF-8',
    'PYTHONDONTWRITEBYTECODE': '1',
    'TMPDIR': '/workspace/qa-review-v030/fix-qa309/tmp-mozlog',
    'MOZBUILD_STATE_PATH': '/workspace/qa-review-v030/tooling/mozbuild-state',
    'RUFF_CACHE_DIR': '/workspace/qa-review-v030/fix-qa309/tmp-mozlog/ruff-cache',
    'PIP_NO_INDEX': '1',
    'UV_OFFLINE': '1',
}
log = EVIDENCE / f'mozlog-{stage}.log'
metadata = EVIDENCE / f'mozlog-{stage}.json'
assert not log.exists() and not metadata.exists()
started = time.monotonic()
with log.open('xb') as output:
    process = subprocess.Popen(argv, cwd=REPO, env=env, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
    timeout = False
    try:
        code = process.wait(timeout=wall)
    except subprocess.TimeoutExpired:
        timeout = True
        os.killpg(process.pid, signal.SIGKILL)
        code = process.wait(timeout=5)
files = {}
for name in FILES:
    data = (REPO / name).read_bytes()
    files[name] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
    if Path(name).name.startswith('test_'):
        files[name]['testMethods'] = len([node for node in ast.walk(ast.parse(data)) if isinstance(node, ast.FunctionDef) and node.name.startswith('test_')])
result = {
    'phase': stage, 'command': argv, 'cwd': str(REPO), 'exit': code, 'timeout': timeout,
    'seconds': round(time.monotonic() - started, 3),
    'limits': {'AS': memory, 'CPU': cpu, 'FSIZE': 8388608, 'wall': wall},
    'envKeys': sorted(env), 'files': files,
    'log': str(log), 'logSha256': hashlib.sha256(log.read_bytes()).hexdigest(),
    'native': False, 'network': False,
}
with metadata.open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(json.dumps(result))
print(log.read_text())
sys.exit(code or int(timeout))
