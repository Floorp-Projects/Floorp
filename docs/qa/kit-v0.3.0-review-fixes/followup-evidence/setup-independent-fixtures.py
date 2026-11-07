import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock

REPO = Path('/workspace/qa-review-v030/worktrees/Runtime-shadow-fixes')
BASE = '81c92fc4bbbc13b4fd4c2a2adaf6a137e342bd53'
SCRIPTS = REPO / '.github/workflows/scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('setup_independent_tests', SCRIPTS / 'test_qa3_build_profile.py')
tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tests)
from qa3_build_profile import profile

case = tests.BuildProfileTests()
rows = []
env = {'PATH': os.environ['PATH'], 'GIT_CONFIG_GLOBAL': '/dev/null', 'GIT_CONFIG_NOSYSTEM': '1', 'GIT_OPTIONAL_LOCKS': '0'}

def base_bytes(path):
    return subprocess.check_output(['git', '-c', 'core.packedGitWindowSize=1m',
                                    '-c', 'core.packedGitLimit=64m', 'show', BASE + ':' + path],
                                   cwd=REPO, env=env, timeout=10)

with tempfile.TemporaryDirectory(prefix='setup-independent-base-') as dirname:
    baseline = Path(dirname)
    for name in ('setup-floorp.sh', 'qa3_build_profile.py'):
        target = baseline / '.github/workflows/scripts' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(base_bytes('.github/workflows/scripts/' + name))
    for source in (REPO / '.github/workflows/mozconfigs').glob('*.mozconfig'):
        relative = source.relative_to(REPO).as_posix()
        original = base_bytes(relative)
        assert original == source.read_bytes(), relative
        target = baseline / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(original)
    combinations = []
    for platform, arch in tests.TARGETS:
        combinations.append(('canonical-debug', platform, arch, True, False, '', ''))
        combinations.append(('production-opt', platform, arch, False, False, '', ''))
        if (platform, arch) != ('linux', 'aarch64'):
            for mode in ('generate', 'use'):
                artifact = 'fixture-profile' if mode == 'use' else ''
                combinations.append(('production-pgo', platform, arch, False, True, mode, artifact))
                combinations.append(('legacy-debug-pgo', platform, arch, True, True, mode, artifact))
    for label, *args in combinations:
        actual = case.setup_result(*args)
        with mock.patch.object(tests, 'ROOT', baseline):
            prior = case.setup_result(*args)
        assert actual['process'].returncode == prior['process'].returncode == 0, (label, args)
        assert actual['config'] == prior['config'], (label, args)
        expected = profile(*args, allow_legacy_debug_pgo=args[2] and args[3])
        assert type(expected['debug']) is bool and type(expected['pgo']) is bool
        assert expected['qualificationEligible'] == (label == 'canonical-debug')
        rows.append({'fixture': 'baseline-full-config-byte-equivalence', 'label': label, 'args': args,
                     'actualExit': actual['process'].returncode, 'baseExit': prior['process'].returncode,
                     'actualConfig': actual['config'], 'configSha256': hashlib.sha256(actual['config'].encode()).hexdigest(),
                     'actualEffects': actual['effects'], 'baseEffects': prior['effects'],
                     'actualStderr': actual['process'].stderr, 'role': expected['role'],
                     'qualificationEligible': expected['qualificationEligible'], 'equal': True})

invalid = [
    ('linux', 'aarch64', False, True, 'use', 'profile'),
    ('linux', 'aarch64', True, True, 'generate', ''),
    ('windows', 'aarch64', False, True, 'use', 'profile'),
    ('mac', 'arm64', False, False, '', ''),
    ('linux', 'x86_64', 'TRUE', False, '', ''),
    ('linux', 'x86_64', False, '0', '', ''),
    ('linux', 'x86_64', False, False, '', 'profile'),
    ('linux', 'x86_64', False, True, 'use', ''),
]
for args in invalid:
    platform, arch, debug, pgo, mode, artifact = args
    arguments = [platform, arch,
                 str(debug).lower() if type(debug) is bool else str(debug),
                 str(pgo).lower() if type(pgo) is bool else str(pgo),
                 mode, artifact, '20261009000000']
    actual = case.setup_result(*args, arguments=arguments)
    assert actual['process'].returncode != 0
    assert actual['config'] is None and actual['effects'] == ''
    assert actual['updateUnchanged'] and not actual['brandingCopied']
    rows.append({'fixture': 'real-setup-early-reject', 'args': args, 'arguments': arguments, 'exit': actual['process'].returncode,
                 'stdout': actual['process'].stdout, 'stderr': actual['process'].stderr,
                 'effects': actual['effects'], 'config': actual['config'], 'updateUnchanged': actual['updateUnchanged'],
                 'brandingCopied': actual['brandingCopied']})

for kwargs in [
    {'debug': 'false'}, {'debug': 0}, {'pgo': 1}, {'pgo': None},
    {'allow_legacy_debug_pgo': 'false'}, {'mode': None}, {'artifact': []},
]:
    values = {'platform': 'linux', 'arch': 'x86_64', 'debug': False, 'pgo': False, **kwargs}
    try:
        profile(**values)
    except ValueError as error:
        rows.append({'fixture': 'typed-profile-contract', 'kwargs': kwargs, 'reject': str(error)})
    else:
        raise AssertionError(values)

print(json.dumps({'schemaVersion': 1, 'base': BASE, 'fixtures': rows, 'fixtureCount': len(rows),
                  'equivalenceCount': len(combinations), 'network': False, 'native': False, 'readOnly': True}, indent=2))
