#!/usr/bin/env python3
"""Publish only small reproducible QA evidence, excluding products and browser state."""
import json,os,re,shutil
from pathlib import Path
src=Path(__file__).resolve().parent
dst=Path('/workspace/qa-linux-package-pwa/docs/qa/linux-22/package-pwa/wayland-browser-identity')
skip={'original-issue.json','original-issue-comments.json','qa-commit.log','qa-push.log'}
allowed={'.json','.md','.py','.log','.txt','.png','.js','.cjs','.mjs','.mts','.strace'}
files=[]
for directory,dirs,names in os.walk(src):
 dirs[:]=sorted(d for d in dirs if d not in {'home','profile','runtime','desktop-home','__pycache__','node_modules','products'} and not d.endswith(('-home','-profile')) and not d.startswith('candidate-product'))
 for name in sorted(names):
  p=Path(directory)/name
  if p.is_symlink() or name in skip or p.suffix not in allowed or p.stat().st_size>1500000:continue
  rel=p.relative_to(src)
  if p.suffix!='.png':
   t=p.read_text(errors='replace')
   if re.search(r'https?://[^\s"<>]+[?&](?:sig|token|se|sv)=',t):raise RuntimeError('Refusing signed URL '+str(rel))
   if re.search(r'"(?:email|emailAddress|avatar_url|avatarUrls|login)"\s*:',t):raise RuntimeError('Refusing personal metadata '+str(rel))
  target=dst/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target);files.append({'path':str(rel),'bytes':p.stat().st_size})
(dst/'published-files.json').write_text(json.dumps(files,indent=2)+'\n')
print(json.dumps({'files':len(files),'bytes':sum(f['bytes'] for f in files)}))
