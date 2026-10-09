import copy
import hashlib
import io
import json
import sys
import tempfile
import time
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

ROOT = Path('/workspace/qa-review-v030/repos/Floorp-Runtime')
EVIDENCE = Path('/workspace/qa-review-v030/fix-qa309/evidence')
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / '.github/workflows/scripts'))
import qa3_mozlog as target

CASE = {'suite': 'xpcshell', 'id': 'independent/test.js', 'minimumAssertions': 1}
PROCESS = {'exit': 0, 'stop': None, 'ownedCleanupComplete': True}
EVENTS = [
    {'action': 'suite_start', 'tests': [CASE['id']]},
    {'action': 'test_start', 'test': CASE['id']},
    {'action': 'test_status', 'test': CASE['id'], 'status': 'PASS', 'subtest': 'base'},
    {'action': 'test_end', 'test': CASE['id'], 'status': 'OK'},
    {'action': 'suite_end'},
]


def encoded(events, ending=b'\n'):
    return ending.join(json.dumps(event, ensure_ascii=False, separators=(',', ':')).encode() for event in events) + ending


def line(size, ending=b'\n', multibyte=False):
    event = {'action': 'log', 'level': 'INFO', 'message': ''}
    empty_size = len(encoded([event], ending))
    padding = size - empty_size
    assert padding >= 0
    event['message'] = ('雪' * (padding // 3) + 'x' * (padding % 3)) if multibyte else 'x' * padding
    result = encoded([event], ending)
    assert len(result) == size
    return result


def outcome(operation):
    try:
        return {'accepted': True, 'result': operation()}
    except ValueError as error:
        return {'accepted': False, 'error': str(error)}


results = {'schemaVersion': 1, 'files': [], 'stream': [], 'count': [], 'metadata': [], 'nativeExecuted': False, 'credentialsInherited': False}
with tempfile.TemporaryDirectory(prefix='mozlog-independent-work-', dir=EVIDENCE) as directory:
    package = Path(directory)
    raw = package / 'raw.jsonl'

    def verify_bytes(label, data, should_accept, **kwargs):
        raw.write_bytes(data)
        observed = outcome(lambda: target.verify_mozlog(raw, CASE, package, PROCESS, **kwargs))
        results['files'].append({'label': label, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'shouldAccept': should_accept, **observed})

    for index, value in enumerate((None, '', ' ', '\t\r\n', 'assertion 雪 🦊', '\u0000')):
        events = copy.deepcopy(EVENTS)
        events[2]['subtest'] = value
        verify_bytes('formal-valid-subtest-' + str(index), encoded(events), True)
    for label, fields in (
        ('missing', {}), ('integer', {'subtest': 1}), ('negative-integer', {'subtest': -1}),
        ('float', {'subtest': 0.0}), ('true', {'subtest': True}), ('false', {'subtest': False}),
        ('empty-list', {'subtest': []}), ('list', {'subtest': ['name']}), ('dict', {'subtest': {}}),
    ):
        events = copy.deepcopy(EVENTS)
        del events[2]['subtest']
        events[2].update(fields)
        verify_bytes('formal-invalid-subtest-' + label, encoded(events), False)

    prefix = encoded(EVENTS)
    limit = target.MAX_EVENT_BYTES
    assert limit == 1024**2
    for ending, name in ((b'\n', 'LF'), (b'\r\n', 'CRLF')):
        for size in (limit - 1, limit, limit + 1):
            verify_bytes('byte-bound-' + name + '-' + str(size), prefix + line(size, ending), size <= limit)
    for size in (limit, limit + 1):
        verify_bytes('utf8-byte-bound-' + str(size), prefix + line(size, multibyte=True), size <= limit)
    verify_bytes('complete-CRLF', encoded(EVENTS, b'\r\n'), True)
    for label, data in (
        ('missing-last-LF', prefix[:-1]),
        ('valid-JSON-at-EOF-without-LF', prefix + b'{"action":"log","level":"INFO"}'),
        ('partial-JSON', prefix + b'{"action":"log"'),
        ('CR-only-at-EOF', prefix + b'{"action":"log","level":"INFO"}\r'),
        ('truncated-UTF8-with-LF', prefix + b'{"action":"log","message":"\xf0\x9f"}\n'),
        ('truncated-UTF8-without-LF', prefix + b'{"action":"log","message":"\xf0\x9f'),
    ):
        verify_bytes(label, data, False)

    data = prefix + line(3 * limit + 7)
    raw.write_bytes(data)
    real_open = Path.open
    trace = {'requests': [], 'returned': [], 'parsedLineBytes': []}
    real_parse = target.parse_rest_json

    class TrackingReader:
        def __init__(self, wrapped):
            self.wrapped = wrapped

        def __enter__(self):
            return self

        def __exit__(self, *args):
            return self.wrapped.__exit__(*args)

        def __iter__(self):
            raise AssertionError('unbounded iteration')

        def readline(self, size=-1):
            assert size == limit + 1
            trace['requests'].append(size)
            value = self.wrapped.readline(size)
            trace['returned'].append(len(value))
            return value

    def open_tracked(path, *args, **kwargs):
        return TrackingReader(real_open(path, *args, **kwargs))

    def parse_tracked(value):
        trace['parsedLineBytes'].append(len(value))
        return real_parse(value)

    with mock.patch.object(Path, 'open', open_tracked), mock.patch.object(target, 'parse_rest_json', parse_tracked):
        observed = outcome(lambda: target.verify_mozlog(raw, CASE, package, PROCESS))
    results['stream'].append({'label': 'actual-3MiB-line-bounded-before-parse', 'inputBytes': len(data), 'shouldAccept': False, **trace, **observed})
    assert trace['requests'] == [limit + 1] * 6
    assert trace['returned'][-1] == limit + 1
    assert len(trace['parsedLineBytes']) == 5

    rows = encoded(EVENTS).splitlines(keepends=True)
    padding = b'{"action":"log","level":"INFO"}\n'

    class CountStream:
        def __init__(self, total):
            self.total = total
            self.returned = 0
            self.calls = 0
            self.max_returned_bytes = 0
            self.sizes = set()

        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

        def __iter__(self):
            raise AssertionError('unbounded iteration')

        def readline(self, size=-1):
            assert size == limit + 1
            self.calls += 1
            self.sizes.add(size)
            if self.returned == self.total:
                return b''
            value = rows[self.returned] if self.returned < len(rows) else padding
            self.returned += 1
            self.max_returned_bytes = max(self.max_returned_bytes, len(value))
            return value

    for count in (1000000, 1000001):
        raw.write_bytes(prefix)
        stream = CountStream(count)
        parsed = [0]

        def count_parse(value):
            parsed[0] += 1
            return real_parse(value)

        started = time.monotonic()
        with mock.patch.object(Path, 'open', return_value=stream), mock.patch.object(target, 'parse_rest_json', count_parse):
            observed = outcome(lambda: target.verify_mozlog(raw, CASE, package, PROCESS))
        results['count'].append({'events': count, 'shouldAccept': count == 1000000, 'readCalls': stream.calls, 'returnedEvents': stream.returned, 'parseCalls': parsed[0], 'requestedSizes': sorted(stream.sizes), 'maxReturnedBytes': stream.max_returned_bytes, 'elapsedSeconds': time.monotonic() - started, 'syntheticLazyStream': True, **observed})
        assert parsed[0] == 1000000

    class BudgetPath:
        def __init__(self, size):
            self.size = size
            self.opens = 0

        def is_symlink(self):
            return False

        def is_file(self):
            return True

        def stat(self):
            return SimpleNamespace(st_size=self.size)

        def open(self, mode):
            assert mode == 'rb'
            self.opens += 1
            return io.BytesIO(prefix)

    for size in (0, 2 * 1024**3, 2 * 1024**3 + 1):
        path = BudgetPath(size)
        observed = outcome(lambda: target.verify_mozlog(path, CASE, package, PROCESS))
        results['metadata'].append({'statSize': size, 'shouldAccept': size == 2 * 1024**3, 'openCalls': path.opens, 'syntheticMetadataOnly': True, **observed})
        if size != 2 * 1024**3:
            assert path.opens == 0

all_cases = [case for group in ('files', 'stream', 'count', 'metadata') for case in results[group]]
results['caseCount'] = len(all_cases)
results['mismatches'] = [case for case in all_cases if case['accepted'] != case['shouldAccept']]
with (EVIDENCE / 'mozlog-independent-fixture-results.json').open('x') as output:
    json.dump(results, output, indent=2)
    output.write('\n')
print(json.dumps({'caseCount': results['caseCount'], 'mismatches': len(results['mismatches']), 'countBoundary': results['count']}))
if results['mismatches']:
    raise SystemExit(1)
