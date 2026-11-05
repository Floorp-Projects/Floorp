#!/usr/bin/env python3
"""Read-only generated-resource comparison; does not execute or patch products."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path, required=True)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
pattern = re.compile(rb'--tab-margin-block:\s*var\(--tab-block-margin\)')
result = {}
for label, product in [('baseline', args.baseline), ('candidate', args.candidate)]:
    entries = []
    with zipfile.ZipFile(product/'omni.ja') as archive:
        for name in archive.namelist():
            if '/nora-content/assets/' not in name or not name.endswith('.js'):
                continue
            data = archive.read(name)
            count = len(pattern.findall(data))
            if count:
                entries.append({'entry': name, 'aliasCount': count,
                                'sha256': hashlib.sha256(data).hexdigest()})
    result[label] = entries
result['scope'] = 'Generated application resources only; not native Windows or runtime UI qualification'
result['sourceSha'] = '214ba6657a63314594de3867785441558b7a63b5'
result['expectedAliasOnlyInCandidate'] = not result['baseline'] and bool(result['candidate'])
args.out.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
raise SystemExit(0 if result['expectedAliasOnlyInCandidate'] else 1)
