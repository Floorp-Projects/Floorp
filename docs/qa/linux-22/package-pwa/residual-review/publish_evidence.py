#!/usr/bin/env python3
"""Publish bounded seven-issue QA evidence, never browser state or opaque redirects."""
import os,re,json,shutil,hashlib
from pathlib import Path
src=Path(__file__).resolve().parent
dst=Path('/workspace/qa-linux-package-pwa/docs/qa/linux-22/package-pwa/residual-review')
allowed={'.json','.md','.py','.log','.txt','.png','.js','.cjs','.mjs','.ts','.mts','.sh'}
excluded={'raw-local','runtime','run','home','profile','profiles','products','node_modules','__pycache__','generated-profile','generated-files'}
if dst.is_symlink():raise RuntimeError('Unexpected destination symlink')
if dst.exists():shutil.rmtree(dst) # only this task's generated QA evidence directory
files=[]
for directory,dirs,names in os.walk(src):
 dirs[:]=sorted(d for d in dirs if d not in excluded and not d.startswith(('home','profile')) and not d.endswith(('-home','-profile','-runtime')))
 for name in sorted(names):
  p=Path(directory)/name
  if p.is_symlink() or p.suffix not in allowed or p.stat().st_size>1_500_000:continue
  rel=p.relative_to(src)
  if p.suffix!='.png':
   text=p.read_text(errors='replace')
   if re.search(r'"(?:email|emailAddress|avatar_url|avatarUrls|login)"\s*:',text):raise RuntimeError('Personal metadata '+str(rel))
   if re.search(r'https?://[^\s"<>]+[?&](?:sig|token|se|sv)=',text):raise RuntimeError('Signed URL '+str(rel))
  q=dst/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);files.append({'path':str(rel),'bytes':p.stat().st_size})
(dst/'published-files.json').write_text(json.dumps(files,indent=2)+'\n')
old=json.loads((src/'previous-matrix.json').read_text());latest=json.loads((src/'matrix.json').read_text())
assert len(latest)==len(old)==7
assert {x['key'] for x in latest}=={x['key'] for x in old}
shutil.copyfile(src/'latest-original-matrix.json',dst.parent/'matrix.json')
assert len(json.loads((dst.parent/'matrix.json').read_text()))==7
print(json.dumps({'files':len(files),'bytes':sum(x['bytes'] for x in files),'assignedIssueCount':7,'originalLinuxDenominator':22,'noAdditionalKAN24':True}))
