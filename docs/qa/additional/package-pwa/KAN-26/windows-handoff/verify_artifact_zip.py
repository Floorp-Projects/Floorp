#!/usr/bin/env python3
"""Verify the downloaded QA artifact and hash installer members without execution."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

SOURCE = '0e80f14dc439dde7817c0620f0fc112d54dfba80'
RUN = 42120232936

def verify(archive, metadata):
    assert metadata['expired'] is False
    assert metadata['workflow_run']['id'] == RUN
    assert metadata['workflow_run']['head_sha'] == SOURCE
    assert 'unsigned-installer' in metadata['name']
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    assert metadata['digest'] == 'sha256:' + digest
    assert archive.stat().st_size == metadata['size_in_bytes']
    members = []
    with zipfile.ZipFile(archive) as bundle:
        for entry in bundle.infolist():
            if entry.is_dir():
                continue
            with bundle.open(entry) as stream:
                member_digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            members.append({'name': entry.filename, 'bytes': entry.file_size,
                            'sha256': member_digest})
    installers = [m for m in members if m['name'].lower().endswith('.exe')]
    assert installers, 'No final installer EXE member'
    return {'sourceSha': SOURCE, 'run': RUN, 'artifact': metadata['id'],
            'artifactName': metadata['name'], 'zipSha256': digest,
            'zipBytes': archive.stat().st_size, 'members': members,
            'installerMembers': installers, 'executed': False}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.archive, json.loads(args.metadata.read_text()))
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
