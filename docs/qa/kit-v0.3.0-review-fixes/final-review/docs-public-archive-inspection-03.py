"""Read a completed archive; do not execute any bundled or packaging code."""

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import resource
import stat
from urllib.parse import parse_qsl, urlsplit
import zipfile

resource.setrlimit(resource.RLIMIT_AS, (256 * 1024**2,) * 2)
resource.setrlimit(resource.RLIMIT_CPU, (20,) * 2)
resource.setrlimit(resource.RLIMIT_FSIZE, (4 * 1024**2,) * 2)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
phase = Path('/workspace/qa-review-v030/fix-qa309')
docs = Path('/workspace/qa-review-v030/worktrees/Floorp-kit-fixes/docs/qa/kit-v0.3.0-review-fixes')
result_dir = Path('/workspace/qa-review-v030/fix-qa309-final-review')
sha = lambda data: hashlib.sha256(data).hexdigest()

def read(path, max_bytes=4 * 1024**2):
    if not stat.S_ISREG(path.lstat().st_mode) or path.stat().st_size > max_bytes:
        raise ValueError('nonregular or oversized review input: ' + str(path))
    return path.read_bytes()

archive_path = docs / 'FOLLOWUP_QA309_Evidence.zip'
archive_data = read(archive_path)
assert len(archive_data) == 782560
assert sha(archive_data) == '2c91b368a2f03ef4ec3267575ded489c4c5827d9db62810567b365132f397d17'
index_data = read(docs / 'followup-evidence-index.json')
assert sha(index_data) == 'd862a8aea7f2851d340d596a318bc0a1dc89e63b545a106bc7fb0c89ec313416'
index = json.loads(index_data)
assert len(index['files']) == 345
assert index['publicationAuthorized'] is False
assert index['nativeQualification'] == 'UNVERIFIED'
assert index['productArtifactQualification'] == 'UNVERIFIED'
expected = {'FOLLOWUP_QA309.md', 'followup-evidence-index.json'} | {r['path'] for r in index['files']}
assert len(expected) == 347
copied_files = set()
for copied in (docs / 'followup-evidence').rglob('*'):
    assert not copied.is_symlink()
    if copied.is_file():
        copied_files.add(copied.relative_to(docs).as_posix())
assert copied_files == {r['path'] for r in index['files']}
records = []
urls = []
with zipfile.ZipFile(archive_path) as z:
    infos = z.infolist()
    assert len(infos) == 347
    assert len({i.filename for i in infos}) == 347
    assert {i.filename for i in infos} == expected
    assert sum(i.file_size for i in infos) == 3790824
    for i in infos:
        p = PurePosixPath(i.filename)
        assert not p.is_absolute() and '..' not in p.parts and '\\' not in i.filename
        assert str(p) == i.filename and '\0' not in i.filename
        assert not (i.flag_bits & 1)
        assert stat.S_IFMT(i.external_attr >> 16) == stat.S_IFREG
        assert stat.S_IMODE(i.external_attr >> 16) == 0o644
        assert i.file_size <= 4 * 1024**2
        data = z.read(i)
        assert data == read(docs / i.filename)
        if i.filename not in {'FOLLOWUP_QA309.md', 'followup-evidence-index.json'}:
            item = next(r for r in index['files'] if r['path'] == i.filename)
            assert len(data) == item['bytes'] and sha(data) == item['sha256']
            local_name = PurePosixPath(i.filename).relative_to('followup-evidence')
            source = phase / 'fixtures' / local_name if str(local_name) in {'run_checks.py', 'package_public_evidence.py'} else phase / 'evidence' / local_name
            assert data == read(source)
            assert not any(part in {'.git', '.hg', '__pycache__', 'node_modules', '.cache', '_virtualenvs', '.venv'} for part in local_name.parts)
            assert not str(local_name).startswith('private-')
            assert str(local_name) not in {'floorp-ci-docs-raw-first.error.log', 'floorp-ci-source-raw-first.error.log'}
        text = data.decode('utf-8')
        assert not re.search(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)', text)
        for url in re.findall(r'https?://[^\s<>"\x27]+', text):
            parsed = urlsplit(url)
            keys = sorted({k.lower() for k, _ in parse_qsl(parsed.query)})
            assert not set(keys) & {'sig', 'token', 'x-amz-signature', 'x-goog-signature', 'sv', 'se', 'skt'}
            if 'blob.core.windows.net' in (parsed.hostname or ''):
                assert i.filename.endswith('.error.redacted.log') and not keys
            urls.append({'file': i.filename, 'host': parsed.hostname, 'queryKeys': keys})
        records.append({'path': i.filename, 'bytes': len(data), 'sha256': sha(data)})
    assert z.testzip() is None

links = []
followup = read(docs / 'FOLLOWUP_QA309.md').decode('utf-8')
for name in re.findall(r'\]\(([^)]+)\)', followup):
    if not re.match(r'\w+://', name):
        target = docs / name.split('#', 1)[0]
        assert target.is_file()
        links.append(name)
historical = []
for name, size, checksum in [
    ('../../Floorp_QA_Implementation_Kit_v0.3.0.zip', 150471, 'a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50'),
    ('Floorp_QA_Implementation_Kit_v0.3.0_review-fixes.1.zip', 158581, '60ad9a63c9fa7e328cc2da41187c6b725a64ee6144aaac8f66c2ebc3260710f2'),
    ('COMPREHENSIVE_REVIEW_QA308.md', 24502, '26b8b073b2cdf3793b57f858634ecb150b6c4150c033b4f514ff2170a0bcc84f'),
    ('COMPREHENSIVE_REVIEW_QA308_Evidence.zip', 2238033, '60b5386d5363edafc4fac738e32c7dd59336db2d23fffb2ba8940a81f65c91d2'),
]:
    value = read(docs / name)
    assert len(value) == size and sha(value) == checksum
    historical.append({'path': name, 'bytes': len(value), 'sha256': sha(value)})

receipt = json.loads(read(docs / 'followup-archive-validation.json'))
assert receipt['bytes'] == len(archive_data) and receipt['sha256'] == sha(archive_data)
assert receipt['entries'] == 347 and receipt['expandedBytes'] == 3790824
assert receipt['indexSha256'] == sha(index_data)
assert receipt['crc'] == 'PASS' and receipt['pathsTypesEncryption'] == 'PASS'
result = {
    'status': 'PASS', 'scope': 'Final documentation and public review archive only',
    'archive': {'bytes': len(archive_data), 'sha256': sha(archive_data), 'entries': 347, 'expandedBytes': 3790824},
    'index': {'bytes': len(index_data), 'sha256': sha(index_data), 'dataFiles': 345},
    'archiveIndexCopiedSourceByteComparisons': 345, 'crc': 'PASS', 'safeRegularPathsAndModes': 'PASS',
    'publicCredentialPatternScan': 'PASS; bounded declared patterns, not general secret proof',
    'temporaryPrivateUrlsAndCachesSelected': False, 'urlMetadataWithoutValues': urls,
    'followupRelativeLinksExist': links, 'historicalArtifacts': historical,
    'files': records,
    'docs': [{'path': n, 'bytes': len(read(docs / n)), 'sha256': sha(read(docs / n))} for n in ['FOLLOWUP_QA309.md', 'README.md', '.gitattributes']],
    'packagerOrBundledCodeExecuted': False, 'sourceOrPhaseEvidenceMutation': False,
    'archiveExtraction': False, 'nativeQualification': 'UNVERIFIED', 'publicationAuthorized': False,
    'limits': {'asBytes': 256 * 1024**2, 'cpuSeconds': 20, 'fileBytes': 4 * 1024**2},
}
with (result_dir / 'docs-public-archive-inspection-03.json').open('x') as f:
    json.dump(result, f, indent=2)
    f.write('\n')
print(json.dumps({k:v for k,v in result.items() if k not in {'files', 'urlMetadataWithoutValues'}}, indent=2))
