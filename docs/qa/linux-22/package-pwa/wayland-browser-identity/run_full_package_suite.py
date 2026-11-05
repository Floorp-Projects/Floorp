#!/usr/bin/env python3
"""Run the six measured identity scenarios only on an authenticated full package."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--intake',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--expected-sha',required=True)
parser.add_argument('--expected-run',type=int,required=True)
parser.add_argument('--display',default=':96')
parser.add_argument('--port-base',type=int,default=2894)
args=parser.parse_args();root=Path(__file__).resolve().parent
meta=json.loads(args.intake.read_text())
assert meta['sourceSha']==args.expected_sha and meta['run']['id']==args.expected_run
assert meta['run']['status']=='completed' and meta['run']['conclusion']=='success'
assert not args.out.exists(),'Refusing to reuse a full-package suite root'
args.out.mkdir(parents=True)
result={'kind':'authenticated full application package; no resource substitutions',
 'sourceSha':meta['sourceSha'],'runId':meta['run']['id'],'artifactId':meta['artifact']['id'],
 'artifactDigest':meta['artifact']['digest'],'groups':[],'qualified':False}
for index,(name,flag) in enumerate([('orders',None),('custom-control','--custom-control-only'),('private-control','--private-control-only')]):
 out=args.out/name
 command=['dbus-run-session','--',sys.executable,str(root/'probe_orders.py'),
  '--intake',str(args.intake),'--out',str(out),'--display',args.display,
  '--port',str(args.port_base+index*2),'--fixture-port',str(8810+index*2)]
 if flag:command.append(flag)
 with (args.out/(name+'.log')).open('w') as log:
  process=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=600)
 row={'name':name,'command':command,'executionReturncode':process.returncode}
 if (out/'result.json').exists():
  raw=json.loads((out/'result.json').read_text());row['completed']=raw['completed'];row['error']=raw.get('error')
  row['productUnchanged']=raw['productUnchanged'];row['remainingOwnedProcesses']=raw['remainingOwnedProcesses']
  if raw['orders'] and (out/'native-protocol.json').exists():
   projection=subprocess.run([sys.executable,str(root/'project_orders.py'),'--result',str(out/'result.json')],capture_output=True,text=True)
   (args.out/(name+'-projection.log')).write_text(projection.stdout+projection.stderr)
   if projection.returncode==0:
    data=json.loads((out/'identity-projection.json').read_text())
    row['cases']=[{'case':case['case'],'matches':case['finalAllMatch'],'quit':case['quit']} for case in data['cases']]
 result['groups'].append(row)
 (args.out/'suite-result.json').write_text(json.dumps(result,indent=2))
result['qualified']=all(row['executionReturncode']==0 and row.get('completed') is True and row.get('productUnchanged') is True
 and not row.get('remainingOwnedProcesses') and row.get('cases') and all(case['matches'] for case in row['cases']) for row in result['groups'])
result['caseCount']=sum(len(row.get('cases',[])) for row in result['groups'])
(args.out/'suite-result.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'qualified':result['qualified'],'caseCount':result['caseCount'],'sourceSha':result['sourceSha'],'runId':result['runId']}))
raise SystemExit(0 if result['qualified'] else 1)
