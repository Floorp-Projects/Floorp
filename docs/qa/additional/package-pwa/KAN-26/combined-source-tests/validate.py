#!/usr/bin/env python3
"""Validate the exact combined-source result without replaying product tests."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parent
meta=json.loads((r/'build-provenance.json').read_text());result=json.loads((r/'result.json').read_text())
assert meta['sourceSha']=='3a9f99ae12dbb221f63b28ec210b04fafcf2be15'
assert meta['sourceTrackedChanges']==''
assert hashlib.sha256((r/'candidate-test.js').read_bytes()).hexdigest()==meta['bundleSha256']
a=result['variants']['candidate'];assert a['allPass'] and a['declaredCaseCount']==4
assert [x['module'] for x in a['modules']]==['lepton-container-geometry','fluerial-container.test.ts']
for x in a['modules']:
 assert x['pass'] and x['progress']['status']=='done' and x['progress']['index']==x['progress']['total']==2
assert result['identity']['sandbox']==6 and not result['identity']['sandboxDisable']
assert result['identity']['version']=='159.0.1' and result['identity']['buildID']=='20261103153232'
assert result['browserExit']==0 and result['quit']['forced'] is False
assert result['ownedProcessesRemaining']==0 and result['allProductFilesUnchanged']
assert not Path('/tmp/.X114-lock').exists() and not Path('/tmp/.X11-unix/X114').exists()
print('Exact3a9f99ae combined source: both2-case modules passed; safe6 normalquit/hash/cleanup verified')
