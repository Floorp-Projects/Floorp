#!/usr/bin/env python3
"""Authenticate exact Actions installer ZIP and stream its tar to a fresh root."""
import argparse,hashlib,json,tarfile,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zip',type=Path,required=True);p.add_argument('--metadata',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--expected-sha',required=True);p.add_argument('--expected-run',type=int,required=True);a=p.parse_args()
meta=json.loads(a.metadata.read_text());run=meta['run'];artifact=meta['artifact']
assert run['id']==a.expected_run and run['head_sha']==a.expected_sha and run['status']=='completed' and run['conclusion']=='success'
assert artifact['name']=='noraneko-linux-x86_64-installer-unsigned' and not artifact['expired'];assert artifact['workflow_run']['id']==run['id'] and artifact['workflow_run']['head_sha']==a.expected_sha
zipsha=hashlib.file_digest(a.zip.open('rb'),'sha256').hexdigest();assert artifact['digest']=='sha256:'+zipsha and a.zip.stat().st_size==artifact['size_in_bytes']
assert not a.out.exists(),'Refuse overwrite';a.out.mkdir(parents=True);a.evidence.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.zip) as z:
 assert z.namelist()==['floorp-linux-x86_64.tar.xz']
 with z.open(z.namelist()[0]) as f:tarsha=hashlib.file_digest(f,'sha256').hexdigest()
 names=set()
 with z.open(z.namelist()[0]) as f,tarfile.open(fileobj=f,mode='r|xz') as t:
  for member in t:
   assert member.name not in names;assert Path(member.name).parts[0]=='floorp';names.add(member.name)
   t.extract(member,a.out,filter='data')
product=a.out/'floorp';assert (product/'floorp').is_file() and (product/'omni.ja').is_file()
files=[{'path':str(f.relative_to(product)),'bytes':f.stat().st_size,'sha256':hashlib.file_digest(f.open('rb'),'sha256').hexdigest()}for f in sorted(product.rglob('*'))if f.is_file()]
(a.evidence/'product-inventory-before.json').write_text(json.dumps(files,indent=2)+'\n')
result={'sourceSha':a.expected_sha,'run':run,'artifact':artifact,'archiveZipSha256':zipsha,'packageTarSha256':tarsha,'tarMemberCount':len(names),'product':str(product),'applicationIni':(product/'application.ini').read_text(),'platformIni':(product/'platform.ini').read_text(),'extraction':'fresh root, unique floorp members, standard datafilter; authenticated compressedtar streamed from ZIP without duplicate cache','qualification':'complete existing unsigned application/package pipeline with locked prebuilt native Runtime; no resource substitution/freshGecko/strictrelease qualification'}
(a.evidence/'product-intake.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k]for k in ['sourceSha','archiveZipSha256','packageTarSha256','tarMemberCount','product']}))
