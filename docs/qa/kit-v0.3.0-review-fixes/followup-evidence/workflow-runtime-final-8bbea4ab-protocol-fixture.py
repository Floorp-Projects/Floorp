import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch


EVIDENCE = Path(__file__).resolve().parent
PREFIX = 'workflow-runtime-final-8bbea4ab-'
RECIPE = Path('/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts')
SNAPSHOT = EVIDENCE / (PREFIX + 'qa3_native_io.py')
sys.path.insert(0, str(RECIPE))
spec = importlib.util.spec_from_file_location('workflow_review_native_8bbe', SNAPSHOT)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
SOURCE_SHA = hashlib.sha256(SNAPSHOT.read_bytes()).hexdigest()
assert SOURCE_SHA == '8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e'
RESULT = {
    'sourceSha256': SOURCE_SHA,
    'commit': 'c9027879e053c9031c7f3e27495f6d415d36c10f',
    'priorBaseCommit': 'cc83e6fea6499d6ec89c76ae9837c280556e9d53',
    'nativeExecution': False,
    'network': False,
    'credentials': False,
    'resourceLimits': {'addressBytes': 256 * 1024**2, 'cpuSeconds': 12, 'fileBytes': 16 * 1024**2},
    'cases': [],
}
VALID_RESULT = {
    'name': 'fake-only',
    'argv': ['fixture-only'],
    'exit': 0,
    'stop': None,
    'durationSeconds': 0.001,
    'stdoutSha256': 'a' * 64,
    'stderrSha256': 'b' * 64,
    'ownedCleanupComplete': True,
    'stdoutIsData': False,
}
VALID = {'result': VALID_RESULT, 'error': None, 'budgetUsed': 4}
EXPECTED = {'name': 'fake-only', 'argv': ['fixture-only'], 'stdoutIsData': False}
original_dumps = json.dumps
PREVIOUS_NINE = {
    'empty error object', 'boolean false error', 'numeric zero error',
    'empty string error', 'extra envelope key', 'null success result',
    'scalar success result', 'incomplete result', 'untyped result exit',
}


def wire(name, reply, outcome='reject', *, budget_present=True, kwargs=None,
         budget_after=None, expected_exception=None):
    kwargs = {'expected': EXPECTED} | (kwargs or {})
    return {
        'name': name, 'reply': reply, 'expect': outcome,
        'budget_present': budget_present, 'kwargs': kwargs,
        'budget_after': budget_after, 'expected_exception': expected_exception,
    }


def result_case(name, field, value, **kwargs):
    return wire(name, VALID | {'result': VALID_RESULT | {field: value}}, **kwargs)


ERROR = {'type': 'OSError', 'args': [122, 'fixture quota'], 'errno': 122,
         'filename': None, 'message': 'fixture quota'}
items = [
    wire('valid bounded result', VALID, 'accept', budget_after=4),
    wire('empty wire', b''),
    wire('truncated JSON', b'{"result":'),
    wire('nonobject', b'[]'),
    wire('duplicate budget key', b'{"result":{},"error":null,"budgetUsed":4,"budgetUsed":5}'),
    wire('missing result', {k: v for k, v in VALID.items() if k != 'result'}),
    wire('missing error', {k: v for k, v in VALID.items() if k != 'error'}),
    wire('missing budget', {k: v for k, v in VALID.items() if k != 'budgetUsed'}),
    wire('boolean budget', VALID | {'budgetUsed': True}),
    wire('regressed budget', VALID | {'budgetUsed': 1}),
    wire('oversized budget', VALID | {'budgetUsed': 11}),
    wire('null budget', VALID | {'budgetUsed': None}),
    wire('string budget', VALID | {'budgetUsed': '4'}),
    wire('empty error object', VALID | {'error': {}}),
    wire('boolean false error', VALID | {'error': False}),
    wire('numeric zero error', VALID | {'error': 0}),
    wire('empty string error', VALID | {'error': ''}),
    wire('extra envelope key', VALID | {'extra': True}),
    wire('null success result', VALID | {'result': None}),
    wire('scalar success result', VALID | {'result': 'success'}),
    result_case('incomplete result', 'ownedCleanupComplete', False),
    result_case('untyped result exit', 'exit', True),
    wire('initial OSError', VALID | {'result': None, 'error': ERROR}, budget_after=4,
         expected_exception='OSError'),
    wire('unknown error class', VALID | {'result': None, 'error': ERROR | {'type': 'UnexpectedClass'}}),
    wire('SystemExit zero error', VALID | {'result': None, 'error': ERROR | {'type': 'SystemExit', 'args': [0]}}),
]
assert len(items) == 25

items += [
    result_case('integer nonzero default reject', 'exit', 1),
    result_case('integer nonzero explicit accept', 'exit', 1, outcome='accept', kwargs={'allow_nonzero': True}, budget_after=4),
    result_case('negative signal exit explicit accept', 'exit', -9, outcome='accept', kwargs={'allow_nonzero': True}, budget_after=4),
    result_case('boolean exit even with explicit allow', 'exit', True, kwargs={'allow_nonzero': True}),
    result_case('float exit even with explicit allow', 'exit', 0.0, kwargs={'allow_nonzero': True}),
    result_case('string exit even with explicit allow', 'exit', '0', kwargs={'allow_nonzero': True}),
    result_case('null exit even with explicit allow', 'exit', None, kwargs={'allow_nonzero': True}),
    wire('string allow_nonzero rejects before fork', VALID, kwargs={'allow_nonzero': 'true'}),
    wire('integer allow_nonzero rejects before fork', VALID, kwargs={'allow_nonzero': 1}),
    result_case('numeric cleanup flag', 'ownedCleanupComplete', 1),
    result_case('non-null stop with nonzero allowed', 'stop', 'TIMEOUT', kwargs={'allow_nonzero': True}),
    result_case('false stop still non-null', 'stop', False),
    result_case('negative duration', 'durationSeconds', -0.1),
    result_case('boolean duration', 'durationSeconds', True),
    result_case('string duration', 'durationSeconds', '0.1'),
    result_case('nonfinite JSON duration', 'durationSeconds', float('nan')),
    result_case('changed command name', 'name', 'changed'),
    result_case('changed command argv', 'argv', ['changed']),
    result_case('empty argv', 'argv', []),
    result_case('nonstring argv item', 'argv', [False]),
    result_case('changed stdout data role', 'stdoutIsData', True),
    result_case('numeric stdout data role', 'stdoutIsData', 0),
    result_case('uppercase stream hash', 'stdoutSha256', 'A' * 64),
    result_case('short stream hash', 'stdoutSha256', 'a' * 63),
    result_case('nonhex stream hash', 'stderrSha256', 'z' * 64),
    result_case('numeric stream hash', 'stderrSha256', 1),
    wire('unexpected success result key', VALID | {'result': VALID_RESULT | {'error': {}}}),
    wire('missing success result key', VALID | {'result': {k: v for k, v in VALID_RESULT.items() if k != 'stderrSha256'}}),
    wire('success result combined with valid error', VALID | {'error': ERROR}),
    wire('missing error message', VALID | {'result': None, 'error': {k: v for k, v in ERROR.items() if k != 'message'}}),
    wire('extra error key', VALID | {'result': None, 'error': ERROR | {'extra': True}}),
    wire('nonstring error message', VALID | {'result': None, 'error': ERROR | {'message': 0}}),
    wire('nonlist error args', VALID | {'result': None, 'error': ERROR | {'args': 'oops'}}),
    wire('boolean error errno', VALID | {'result': None, 'error': ERROR | {'errno': True}}),
    wire('nonstring error filename', VALID | {'result': None, 'error': ERROR | {'filename': False}}),
    wire('BaseException error cannot cause clean exit', VALID | {'result': None, 'error': ERROR | {'type': 'BaseException'}}),
    wire('KeyboardInterrupt error cannot escape as control signal', VALID | {'result': None, 'error': ERROR | {'type': 'KeyboardInterrupt'}}),
    wire('OSError filename and errno retained', VALID | {'result': None, 'error': ERROR | {'filename': '/fixture-only'}}, budget_after=4, expected_exception='OSError'),
    wire('float budget', VALID | {'budgetUsed': 4.0}),
    wire('budget maximum inclusive', VALID | {'budgetUsed': 10}, 'accept', budget_after=10),
    wire('budget previous usage inclusive', VALID | {'budgetUsed': 2}, 'accept', budget_after=2),
    wire('no budget accepts only null', VALID | {'budgetUsed': None}, 'accept', budget_present=False),
    wire('no budget numeric value rejected', VALID | {'budgetUsed': 0}, budget_present=False),
    wire('error cannot bypass invalid budget', VALID | {'result': None, 'error': ERROR, 'budgetUsed': 11}, expected_exception='ValueError'),
    wire('reply oversized', b'{"padding":"' + b'x' * (128 * 1024) + b'"}'),
]


with tempfile.TemporaryDirectory(prefix='workflow-runtime-8bbe-wire-') as directory:
    root = Path(directory)
    for index, case in enumerate(items):
        budget = native.IOBudget(root, 10) if case['budget_present'] else None
        if budget:
            budget.used = 2
        reply = case['reply']
        encoded = reply.decode() if isinstance(reply, bytes) else original_dumps(reply)
        record = {
            'name': case['name'], 'expected': case['expect'],
            'previous25': index < 25, 'previousAcceptedMismatch': case['name'] in PREVIOUS_NINE,
            'returnedNormally': False,
        }
        try:
            with patch.object(native.json, 'dumps', return_value=encoded):
                value = native._supervise(lambda cancelled: copy.deepcopy(VALID_RESULT), 1,
                                          budget, **case['kwargs'])
            record['returnedNormally'] = True
            record['returnedResult'] = value
        except BaseException as error:
            record['raised'] = type(error).__name__
            record['error'] = str(error)
            if isinstance(error, OSError):
                record['errno'] = error.errno
                record['filename'] = error.filename
            if isinstance(error, SystemExit):
                record['systemExitCode'] = error.code
        record['budgetUsedAfter'] = budget.used if budget else None
        record['matchedExpectation'] = record['returnedNormally'] == (case['expect'] == 'accept')
        if case['budget_after'] is not None:
            record['matchedExpectation'] &= record['budgetUsedAfter'] == case['budget_after']
        if case['expected_exception']:
            record['matchedExpectation'] &= record.get('raised') == case['expected_exception']
        if case['name'] == 'OSError filename and errno retained':
            record['matchedExpectation'] &= record.get('errno') == 122 and record.get('filename') == '/fixture-only'
        if case['expect'] == 'reject':
            record['matchedExpectation'] &= record.get('raised') not in {'SystemExit', 'KeyboardInterrupt', 'BaseException'}
        RESULT['cases'].append(record)

    captured = {}

    def capture(command, timeout, budget, **kwargs):
        captured.update(timeout=timeout, budget=budget, kwargs=kwargs)
        return copy.deepcopy(VALID_RESULT)

    with patch.object(native, '_supervise', side_effect=capture):
        value = native.run_owned(['fixture-only'], root, {}, root / 'logs', 'fake-only', 1,
                                 allow_nonzero=True, data_stdout=False)
    public_binding = captured['kwargs'] == {'expected': EXPECTED, 'allow_nonzero': True}
    RESULT['cases'].append({'name': 'public run_owned forwards exact command/data/nonzero policy',
                            'matchedExpectation': public_binding,
                            'capturedKwargs': captured['kwargs'],
                            'invokedTarget': False})

RESULT['total'] = len(RESULT['cases'])
RESULT['previous25Pass'] = all(case['matchedExpectation'] for case in RESULT['cases'] if case.get('previous25'))
RESULT['previousNineNowRejected'] = all(case['matchedExpectation'] and not case['returnedNormally'] for case in RESULT['cases'] if case.get('previousAcceptedMismatch'))
RESULT['typedGuardMismatches'] = [case['name'] for case in RESULT['cases'] if not case['matchedExpectation']]
RESULT['allPass'] = not RESULT['typedGuardMismatches']
RESULT['limitation'] = (
    'Reply mutation is injected only into trusted private producer serialization. '
    'Fake hashes prove strict shape/hex validation, not actual log contents. The unchanged caller '
    'binds pinned tool version output SHA-256 separately. No untrusted native access to control '
    'pipe or qualification/publication bypass is established. Parent initialization, real '
    'owned-process cleanup and external job-scope prerequisites are separately reviewed.'
)
(EVIDENCE / (PREFIX + 'protocol-results.json')).write_text(original_dumps(RESULT, indent=2) + '\n')
print(original_dumps(RESULT, indent=2))
sys.exit(0 if RESULT['allPass'] else 1)
