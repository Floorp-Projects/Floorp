#!/usr/bin/env python3
"""Publish only this additional target; conserve original7 and other addon rows."""
from pathlib import Path
import os,json,hashlib,re,shutil
src=Path(__file__).resolve().parent
repo=Path('/workspace/qa-linux-package-pwa');dst=repo/'docs/qa/additional/package-pwa/KAN-95/pr3052'
allowed={'.json','.md','.py','.sh','.txt','.log','.png','.cjs','.mjs','.mts','.ts','.js','.patch'}
exclude={'profile','profiles','home','runtime','node_modules','__pycache__','old-product','candidate-product','products','generated-profile','raw-local','home-direct','runtime-direct'}
generated={'candidate-test.js','baseline-test.js','candidate.js','proposed.js','canonical.js'}
scope=json.loads((src/'scope.json').read_text());old7=repo/'docs/qa/linux-22/package-pwa/matrix.json';assert hashlib.sha256(old7.read_bytes()).hexdigest()==scope['original7MatrixSha256']
if dst.is_symlink():raise RuntimeError('Unexpected destination symlink')
if dst.exists():shutil.rmtree(dst)
files=[]
for d,dirs,names in os.walk(src):
 dirs[:]=sorted(n for n in dirs if n not in exclude and not n.startswith(('home','profile')) and not n.endswith(('-home','-profile','-runtime')))
 for n in sorted(names):
  p=Path(d)/n
  if p.is_symlink() or n in generated or p.suffix not in allowed or p.stat().st_size>1_500_000:continue
  rel=p.relative_to(src)
  if p.suffix!='.png':
   t=p.read_text(errors='replace')
   if re.search(r'"(?:email|emailAddress|avatar_url|avatarUrls|login)"\s*:',t):raise RuntimeError('Personal metadata '+str(rel))
   if re.search(r'https?://[^\s"<>]+[?&](?:sig|token|se|sv)=',t):raise RuntimeError('Signed URL '+str(rel))
  target=dst/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target);files.append({'path':str(rel),'bytes':p.stat().st_size})
(dst/'published-files.json').write_text(json.dumps(files,indent=2)+'\n')
p=repo/'docs/qa/additional/package-pwa/matrix.json';prior=json.loads(p.read_text())if p.exists()else[];keep=[x for x in prior if x['key']!='KAN-95'];after=keep+json.loads((src/'matrix.json').read_text());assert all(x in after for x in keep);p.write_text(json.dumps(after,ensure_ascii=False,indent=2)+'\n')
assert hashlib.sha256(old7.read_bytes()).hexdigest()==scope['original7MatrixSha256']
print(json.dumps({'files':len(files),'bytes':sum(x['bytes']for x in files),'additionalKeys':[x['key']for x in after],'original7Unchanged':True,'originalLinuxDenominator':22,'excluded':'profiles/home/runtime/native products/compiled large bundles/download URLs'}))
