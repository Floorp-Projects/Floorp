#!/usr/bin/env python3
"""Authenticate a downloaded package Actions ZIP, then safely extract its product.

Input metadata is the compact current API artifact/run record saved by the audit.
This never modifies product contents after extraction and does not run the app.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--zip', type=Path, required=True)
p.add_argument('--metadata', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--evidence', type=Path, required=True)
a = p.parse_args()
meta = json.loads(a.metadata.read_text())
artifact = meta['artifact']
run = meta['run']
expected_sha = '8249e234adfbb3e877e4a20cf5f57d7264d9a447'
assert run['id'] == 42068055038 and run['head_sha'] == expected_sha
assert run['status'] == 'completed' and run['conclusion'] == 'success'
assert artifact['name'] == 'noraneko-linux-x86_64-installer-unsigned'
assert artifact['expired'] is False
assert artifact['workflow_run']['id'] == run['id']
assert artifact['workflow_run']['head_sha'] == expected_sha
zip_sha = hashlib.file_digest(a.zip.open('rb'), 'sha256').hexdigest()
assert artifact['digest'] == 'sha256:' + zip_sha
assert a.zip.stat().st_size == artifact['size_in_bytes']
if a.out.exists() and any(a.out.iterdir()):
    raise RuntimeError('Product output is not empty; refuse to overwrite it')
a.out.mkdir(parents=True, exist_ok=True)
a.evidence.mkdir(parents=True, exist_ok=True)
tar_path = a.evidence / 'floorp-linux-x86_64.tar.xz'
with zipfile.ZipFile(a.zip) as archive:
    assert archive.namelist() == ['floorp-linux-x86_64.tar.xz']
    with archive.open(archive.namelist()[0]) as source, tar_path.open('xb') as target:
        while block := source.read(1024 * 1024):
            target.write(block)
tar_sha = hashlib.file_digest(tar_path.open('rb'), 'sha256').hexdigest()
with tarfile.open(tar_path, 'r:xz') as archive:
    members = archive.getmembers()
    names = [m.name for m in members]
    assert names and len(names) == len(set(names)), 'Empty/duplicate tar entries'
    assert all(Path(n).parts[0] == 'floorp' for n in names)
    # Standard data filter rejects absolute/outside-root links and special files.
    archive.extractall(a.out, filter='data')
product = a.out / 'floorp'
assert (product / 'floorp').is_file() and (product / 'application.ini').is_file()
inventory = []
for file in sorted(product.rglob('*')):
    if file.is_symlink():
        inventory.append({'path':str(file.relative_to(product)), 'symlink':str(file.readlink())})
    elif file.is_file():
        inventory.append({'path':str(file.relative_to(product)), 'size':file.stat().st_size,
            'sha256':hashlib.file_digest(file.open('rb'), 'sha256').hexdigest()})
(a.evidence / 'product-inventory-before.json').write_text(json.dumps(inventory, indent=2))
result = {'sourceSha':expected_sha, 'run':run, 'artifact':artifact,
    'archiveZipSha256':zip_sha, 'packageTarSha256':tar_sha,
    'productRoot':str(product), 'tarEntryCount':len(members),
    'extractionPolicy':'exact one-member ZIP; expected floorp root; standard tar data filter; no overwrite',
    'productInventoryFile':'product-inventory-before.json',
    'applicationIni':(product / 'application.ini').read_text(),
    'platformIni':(product / 'platform.ini').read_text(),
    'signatureScope':'unsigned validation package from exact source; not public signed distribution'}
(a.evidence / 'product-intake.json').write_text(json.dumps(result, indent=2))
print(json.dumps({k:result[k] for k in ['sourceSha','archiveZipSha256','packageTarSha256','productRoot','tarEntryCount']}))
