#!/usr/bin/env python3
"""Validate recorded new residual evidence, without relaunching products."""
from pathlib import Path
import json,sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('result.json')
d=json.loads(p.read_text())
assert d['completed'] and d['cacheReuseAll'] and d['allMarkers']
assert d['localCachePathStable'] and d['xdgSelected'] and d['deployedLauncherUnchanged']
assert len(d['cacheRuns'])==5 and len(d['conflicts'])==3
for i,row in enumerate(d['cacheRuns']):
 assert row['payload']['length']==65555 and row['payload']['prefix']=='KAN8-CACHE-CONTENT'
 assert row['payloadRequestsAfter']==1 and row['cacheEntryCountAfterQuit']>=1
 assert row['payload']['transferSize']==(65855 if i==0 else 0)
 assert row['marker']=='retained-cache-profile' and row['exitCode']==0
 assert row['quit']['in_app'] and not row['quit']['forced']
 assert row['version']=='159.0.1' and row['buildID']=='20261103153232'
 assert row['sandbox']==6 and not row['sandboxDisabled'] and row['diskCacheEnabled']
for row in d['conflicts']:
 assert row['exitCode']==1 and row['diagnosticPresent'] and row['profileFixturesUnchanged']
 assert row['fixtureHashesBefore']==row['fixtureHashesAfter']
assert d['ownedProcessesRemaining']==0 and d['displayClean']
print('PASS: five actual cache-content/profile runs, three installed-launcher guards; no browser rerun')
