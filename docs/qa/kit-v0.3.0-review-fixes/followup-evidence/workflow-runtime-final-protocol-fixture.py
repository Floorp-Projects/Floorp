import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch


EVIDENCE = Path(__file__).resolve().parent
RECIPE = Path('/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts')
SNAPSHOT = EVIDENCE / 'workflow-runtime-final-qa3_native_io.py'
sys.path.insert(0, str(RECIPE))
spec = importlib.util.spec_from_file_location('review_native_io', SNAPSHOT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
RESULT = {'sourceSha256': hashlib.sha256(SNAPSHOT.read_bytes()).hexdigest(), 'nativeExecution': False, 'network': False, 'credentials': False, 'cases': []}
VALID_RESULT = {'name': 'fake-only', 'argv': ['fixture-only'], 'exit': 0, 'stop': None, 'durationSeconds': 0.001, 'stdoutSha256': 'a' * 64, 'stderrSha256': 'b' * 64, 'ownedCleanupComplete': True, 'stdoutIsData': False}
VALID = {'result': VALID_RESULT, 'error': None, 'budgetUsed': 4}
original_dumps = json.dumps


def observe(name, reply, budget, expected):
    if isinstance(reply, bytes):
        encoded = reply.decode()
    else:
        encoded = original_dumps(reply)
    outcome = {'name': name, 'expected': expected, 'returnedNormally': False}
    try:
        with patch.object(native.json, 'dumps', return_value=encoded):
            result = native._supervise(lambda cancelled: VALID_RESULT, 1, budget)
        outcome['returnedNormally'] = True
        outcome['returnedResult'] = result
    except BaseException as error:
        outcome['raised'] = type(error).__name__
        outcome['error'] = str(error)
        if isinstance(error, SystemExit):
            outcome['systemExitCode'] = error.code
    outcome['budgetUsedAfter'] = budget.used if budget else None
    outcome['matchedExpectation'] = outcome['returnedNormally'] == (expected == 'accept')
    RESULT['cases'].append(outcome)


with tempfile.TemporaryDirectory(prefix='workflow-runtime-wire-') as directory:
    root = Path(directory)
    items = [
        ('valid bounded result', VALID, 'accept'),
        ('empty wire', b'', 'reject'),
        ('truncated JSON', b'{"result":', 'reject'),
        ('nonobject', b'[]', 'reject'),
        ('duplicate budget key', b'{"result":{},"error":null,"budgetUsed":4,"budgetUsed":5}', 'reject'),
        ('missing result', {k:v for k,v in VALID.items() if k != 'result'}, 'reject'),
        ('missing error', {k:v for k,v in VALID.items() if k != 'error'}, 'reject'),
        ('missing budget', {k:v for k,v in VALID.items() if k != 'budgetUsed'}, 'reject'),
        ('boolean budget', VALID | {'budgetUsed': True}, 'reject'),
        ('regressed budget', VALID | {'budgetUsed': 1}, 'reject'),
        ('oversized budget', VALID | {'budgetUsed': 11}, 'reject'),
        ('null budget', VALID | {'budgetUsed': None}, 'reject'),
        ('string budget', VALID | {'budgetUsed': '4'}, 'reject'),
        ('empty error object', VALID | {'error': {}}, 'reject'),
        ('boolean false error', VALID | {'error': False}, 'reject'),
        ('numeric zero error', VALID | {'error': 0}, 'reject'),
        ('empty string error', VALID | {'error': ''}, 'reject'),
        ('extra envelope key', VALID | {'extra': True}, 'reject'),
        ('null success result', VALID | {'result': None}, 'reject'),
        ('scalar success result', VALID | {'result': 'success'}, 'reject'),
        ('incomplete result', VALID | {'result': VALID_RESULT | {'ownedCleanupComplete': False}}, 'reject'),
        ('untyped result exit', VALID | {'result': VALID_RESULT | {'exit': True}}, 'reject'),
        ('initial OSError', VALID | {'result': None, 'error': {'type': 'OSError', 'args': [122, 'fixture quota'], 'errno': 122, 'filename': None, 'message': 'fixture quota'}}, 'reject'),
        ('unknown error class', VALID | {'result': None, 'error': {'type': 'UnexpectedClass', 'args': ['fixture error'], 'errno': None, 'filename': None, 'message': 'fixture error'}}, 'reject'),
        ('SystemExit zero error', VALID | {'result': None, 'error': {'type': 'SystemExit', 'args': [0], 'errno': None, 'filename': None, 'message': 'zero'}}, 'reject'),
    ]
    for name, reply, expected in items:
        budget = native.IOBudget(root, 10)
        budget.used = 2
        observe(name, reply, budget, expected)

RESULT['total'] = len(RESULT['cases'])
RESULT['typedGuardMismatches'] = [c['name'] for c in RESULT['cases'] if not c['matchedExpectation']]
RESULT['limitation'] = 'Mutated reply is injected only into the trusted private producer serialization. This tests defensive protocol validation; it does not prove that a real untrusted native target can write the control pipe or bypass a qualification/publication gate.'
(EVIDENCE / 'workflow-runtime-final-protocol-results.json').write_text(original_dumps(RESULT, indent=2) + '\n')
print(original_dumps(RESULT, indent=2))
