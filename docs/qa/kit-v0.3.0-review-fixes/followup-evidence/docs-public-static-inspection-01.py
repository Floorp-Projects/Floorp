"""Read-only inspection; never imports or executes the public packager."""

import hashlib
import json
from pathlib import Path
import re
import resource
import stat
import subprocess
from urllib.parse import parse_qsl, urlsplit
import zipfile

resource.setrlimit(resource.RLIMIT_AS, (256 * 1024**2,) * 2)
resource.setrlimit(resource.RLIMIT_CPU, (20,) * 2)
resource.setrlimit(resource.RLIMIT_FSIZE, (4 * 1024**2,) * 2)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
phase = Path('/workspace/qa-review-v030/fix-qa309')
repo = Path('/workspace/qa-review-v030/worktrees/Floorp-kit-fixes')
docs = repo / 'docs/qa/kit-v0.3.0-review-fixes'
out = phase / 'evidence/docs-public-static-inspection-01.json'
base = 'd9e03b8c4ccf84a52db8a7532fdd8e3fd1157f89'
excluded = {'floorp-ci-docs-raw-first.error.log', 'floorp-ci-source-raw-first.error.log'}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def read(path):
    mode = path.lstat().st_mode
    if not stat.S_ISREG(mode) or path.stat().st_size > 4 * 1024**2:
        raise ValueError('nonregular/oversized inspection input: ' + str(path))
    return path.read_bytes()

def git_blob(relative):
    cmd = ['git', '-c', 'core.packedGitWindowSize=1m', '-c', 'core.packedGitLimit=64m', 'show', base + ':' + relative]
    result = subprocess.run(cmd, cwd=repo, env={'PATH': '/usr/bin:/bin', 'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null'}, capture_output=True, timeout=15)
    return result.stdout if result.returncode == 0 else None

historical_names = [
    'docs/Floorp_QA_Implementation_Kit_v0.3.0.zip',
    'docs/qa/kit-v0.3.0-review-fixes/Floorp_QA_Implementation_Kit_v0.3.0_review-fixes.1.zip',
    'docs/qa/kit-v0.3.0-review-fixes/kit-fixes.patch',
    'docs/qa/kit-v0.3.0-review-fixes/archive-validation.json',
    'docs/qa/kit-v0.3.0-review-fixes/evidence-index.json',
    'docs/qa/kit-v0.3.0-review-fixes/COMPREHENSIVE_REVIEW_QA308.md',
    'docs/qa/kit-v0.3.0-review-fixes/COMPREHENSIVE_REVIEW_QA308_Evidence.zip',
]
historical = []
for name in historical_names:
    p = repo / name
    if not p.exists():
        historical.append({'path': name, 'present': False})
        continue
    data = read(p)
    blob = git_blob(name)
    record = {'path': name, 'present': True, 'bytes': len(data), 'sha256': sha(data), 'trackedAtBase': blob is not None, 'baseBytesUnchanged': data == blob if blob is not None else None}
    if p.suffix == '.zip':
        with zipfile.ZipFile(p) as z:
            record['archiveEntries'] = len(z.infolist())
            record['expandedBytes'] = sum(i.file_size for i in z.infolist())
    historical.append(record)

index = json.loads(read(docs / 'evidence-index.json'))
old_index_checks = []
for name, record in index['files'].items():
    path = docs / 'evidence' / name
    data = read(path)
    old_index_checks.append({'path': name, 'matches': len(data) == record['bytes'] and sha(data) == record['sha256']})

old_zip = repo / 'docs/Floorp_QA_Implementation_Kit_v0.3.0.zip'
fixed_zip = docs / 'Floorp_QA_Implementation_Kit_v0.3.0_review-fixes.1.zip'
kit_docs = []
with zipfile.ZipFile(old_zip) as a, zipfile.ZipFile(fixed_zip) as b:
    for item in a.infolist():
        relative = item.filename.split('/', 1)[1]
        if relative == 'DESIGN.md' or relative.startswith('docs/') and relative.endswith('.md'):
            if item.file_size > 1024**2:
                raise ValueError('oversized historical design')
            before, after = a.read(item.filename), b.read(item.filename)
            kit_docs.append({'path': relative, 'bytes': len(before), 'sha256': sha(before), 'unchanged': before == after})

sources = []
links = []
for p in sorted((phase / 'evidence').iterdir()):
    if p.is_symlink():
        links.append(p.name)
    elif p.is_file() and p.name not in excluded:
        sources.append((p, p.name))
root = phase / 'evidence/floorp-source106-clean-commit-01'
for p in sorted(root.rglob('*')):
    if p.is_symlink():
        links.append(str(p.relative_to(phase / 'evidence')))
    elif p.is_file():
        sources.append((p, str(p.relative_to(phase / 'evidence'))))
sources.append((phase / 'fixtures/run_checks.py', 'run_checks.py'))
url_rows = []
credentials = []
binary_names = []
selected = []
cache_parts = {'.git', '.hg', '__pycache__', 'node_modules', '.cache', '_virtualenvs', '.venv'}
cache_members = []
for path, name in sources:
    data = read(path)
    selected.append({'path': name, 'bytes': len(data), 'sha256': sha(data)})
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        binary_names.append(name)
        text = data.decode('utf-8', errors='replace')
    if cache_parts.intersection(Path(name).parts):
        cache_members.append(name)
    if re.search(r'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)', text):
        credentials.append(name)
    for url in re.findall(r'https?://[^\s<>"\x27]+', text):
        parsed = urlsplit(url)
        keys = sorted({k.lower() for k, _ in parse_qsl(parsed.query)})
        url_rows.append({'file': name, 'host': parsed.hostname, 'queryKeys': keys, 'credentialQuery': bool(set(keys) & {'sig', 'token', 'x-amz-signature', 'x-goog-signature', 'sv', 'se', 'skt'})})

result = {
    'reviewBase': base, 'packagerExecutedOrImported': False, 'sourceMutation': False,
    'historicalInputs': historical, 'oldIndexEntries': len(old_index_checks),
    'oldIndexMismatches': [r for r in old_index_checks if not r['matches']],
    'designAndDocs': kit_docs,
    'packagerDefinition': {'sha256': sha(read(phase / 'fixtures/package_public_evidence.py')), 'selectedAtInspection': len(selected), 'selectedBytes': sum(r['bytes'] for r in selected), 'maxMemberBytes': max(r['bytes'] for r in selected), 'sources': selected},
    'selectedSymlinks': links, 'selectedCacheMembers': cache_members,
    'selectedNonUtf8Members': binary_names, 'selectedCredentialShapeMatches': credentials,
    'selectedUrlMetadataWithoutValues': url_rows,
    'privateFixtureDirectorySelected': False,
    'limits': {'asBytes': 256 * 1024**2, 'cpuSeconds': 20, 'fileBytes': 4 * 1024**2},
    'qualification': 'UNVERIFIED', 'publicationAuthorized': False,
}
with out.open('x') as f:
    json.dump(result, f, indent=2)
    f.write('\n')
print(json.dumps({k: v for k, v in result.items() if k != 'packagerDefinition' and k != 'selectedUrlMetadataWithoutValues'}, indent=2))
print(json.dumps({'selectedCount': len(selected), 'selectedBytes': result['packagerDefinition']['selectedBytes'], 'maxMemberBytes': result['packagerDefinition']['maxMemberBytes'], 'urlCount': len(url_rows), 'credentialUrlCount': sum(r['credentialQuery'] for r in url_rows), 'urlHosts': sorted({r['host'] or '' for r in url_rows})}))
