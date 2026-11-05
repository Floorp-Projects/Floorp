#!/usr/bin/env python3
"""Copy additional-target QA evidence without products, browser state or raw account metadata."""
import hashlib,json,os,re,shutil
from pathlib import Path
src=Path(__file__).resolve().parent
dst=Path('/workspace/qa-linux-package-pwa/docs/qa/additional/package-pwa/KAN-24')
skip={'github-issue.json','github-comments.json','pr2950.json','pr2950-comments.json','pr2950-reviews.json','pr2950-inline.json','pr-search.json','qa-commit.log','qa-push.log','integration-macos-check.json','attempt1-integration-macos-check.json'}
allowed={'.json','.jsonl','.md','.py','.log','.txt','.png','.js','.cjs','.mjs','.mts','.ts','.patch','.sh'}
files=[]
for directory,dirs,names in os.walk(src):
 dirs[:]=sorted(d for d in dirs if d not in {'runtime','migration-seed','__pycache__','node_modules','products'} and not d.startswith(('home','profile','candidate-product')) and not d.endswith(('-profile','-home')))
 for name in sorted(names):
  p=Path(directory)/name
  if p.is_symlink() or name in skip or p.suffix not in allowed or p.stat().st_size>1500000:continue
  rel=p.relative_to(src)
  if p.suffix!='.png':
   t=p.read_text(errors='replace')
   if re.search(r'https?://[^\s"<>]+[?&](?:sig|token|se|sv)=',t):raise RuntimeError('Refusing signed URL '+str(rel))
   if re.search(r'"(?:email|emailAddress|avatar_url|avatarUrls|login)"\s*:',t):raise RuntimeError('Refusing personal metadata '+str(rel))
  q=dst/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);files.append({'path':str(rel),'bytes':p.stat().st_size})
(dst/'published-files.json').write_text(json.dumps(files,indent=2)+'\n')
owner=dst.parent;owner.mkdir(parents=True,exist_ok=True);shutil.copyfile(src/'matrix.json',owner/'matrix.json')
scope=json.loads((src/'scope.json').read_text());old=Path('/workspace/qa-linux-package-pwa/docs/qa/linux-22/package-pwa/matrix.json');assert hashlib.sha256(old.read_bytes()).hexdigest()==scope['originalMatrixSha256']
print(json.dumps({'files':len(files),'bytes':sum(f['bytes'] for f in files),'additionalTargets':1,'original7MatrixUnchanged':True,'originalLinuxDenominator':22}))
