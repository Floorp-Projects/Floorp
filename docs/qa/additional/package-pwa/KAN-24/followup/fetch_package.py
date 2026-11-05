#!/usr/bin/env python3
"""Download exact unsigned package artifact via authenticated gh; preserve SHA evidence."""
import argparse,json,pathlib,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--run',type=int,required=True);p.add_argument('--sha',required=True);p.add_argument('--label',required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parent;out=r/'intake'/a.label;out.mkdir(parents=True,exist_ok=True)
def api(path):return json.loads(subprocess.check_output(['gh','api','repos/Floorp-Projects/Floorp/'+path]))
run=api('actions/runs/'+str(a.run));assert run['head_sha']==a.sha and run['status']=='completed' and run['conclusion']=='success'
items=api(f'actions/runs/{a.run}/artifacts')['artifacts'];art=[x for x in items if x['name']=='noraneko-linux-x86_64-installer-unsigned'];assert len(art)==1;art=art[0]
compactrun={k:run[k] for k in ['id','head_sha','status','conclusion','path','event','html_url','created_at','updated_at']}
compactart={k:art[k] for k in ['id','name','size_in_bytes','digest','expired','created_at','expires_at','workflow_run']}
metadata=out/'api-metadata.json';metadata.write_text(json.dumps({'run':compactrun,'artifact':compactart},indent=2)+'\n')
z=out/'artifact.zip'
if not z.exists():
 with z.open('xb') as f:subprocess.run(['gh','api',f'repos/Floorp-Projects/Floorp/actions/artifacts/{art["id"]}/zip'],stdout=f,check=True)
subprocess.run([sys.executable,str(r/'intake_product.py'),'--zip',str(z),'--metadata',str(metadata),'--out',str(r/'products'/a.label),'--evidence',str(out),'--expected-sha',a.sha,'--expected-run',str(a.run)],check=True)
(out/'download-cache-cleanup.json').write_text(json.dumps({'archiveZipSha256':json.loads((out/'product-intake.json').read_text())['archiveZipSha256'],'packageTarSha256':json.loads((out/'product-intake.json').read_text())['packageTarSha256'],'reason':'verified download cache removed after safe extraction; authenticated immutable product retained; archives re-downloadable by artifact ID'},indent=2)+'\n')
z.unlink()
(out/'floorp-linux-x86_64.tar.xz').unlink()
print(json.dumps({'label':a.label,'run':a.run,'artifact':art['id'],'sha':a.sha,'product':str(r/'products'/a.label/'floorp')}))
