#!/usr/bin/env python3
"""Validate the expected differential outcomes and cleanup contract."""
import json, sys
from pathlib import Path
r=json.loads(Path(sys.argv[1]).read_text())
assert 'error' not in r, r.get('error')
assert len(r['variants']['canonical']['rows'])==4
assert len(r['variants']['proposed']['rows'])==4
assert all(not row['passed'] for row in r['variants']['canonical']['rows'])
assert all(row['passed'] for row in r['variants']['proposed']['rows'])
assert r['identity']['sandbox']==6 and not r['identity']['sandboxDisable']
assert r['quit']['forced'] is False and r['browserExit']==0
assert r['allProductFilesUnchanged'] and r['ownedProcessesRemaining']==0
print('PASS: canonical exposes 4 gaps; proposed fixes 4 API cases; protected product unchanged; normal exit and no owned processes.')
