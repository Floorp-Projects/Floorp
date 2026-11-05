#!/usr/bin/env python3
"""Collect compact standard CI state without account metadata or secrets."""
import json,subprocess,datetime
from pathlib import Path
R=Path(__file__).resolve().parent
run=42110920041
head='3a9f99ae12dbb221f63b28ec210b04fafcf2be15'
def query(path):return json.loads(subprocess.check_output(['gh','api','repos/Floorp-Projects/Floorp/'+path],timeout=25))
x=query(f'actions/runs/{run}');assert x['head_sha']==head
j=query(f'actions/runs/{run}/jobs')['jobs'];jobs=[]
for v in j:
 jobs.append({k:v.get(k) for k in ['id','name','status','conclusion','started_at','completed_at','runner_id','runner_name','check_run_url','steps']})
out={k:x.get(k) for k in ['id','name','head_sha','head_branch','event','path','status','conclusion','run_attempt','html_url','created_at','updated_at']};out['jobs']=jobs;out['observed_at']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(R/'metadata.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'run':run,'status':x['status'],'conclusion':x['conclusion'],'attempt':x['run_attempt'],'jobs':[{'id':v['id'],'name':v['name'],'status':v['status'],'conclusion':v['conclusion'],'active':[s['name'] for s in v['steps'] if s['status']=='in_progress']} for v in jobs]}))
