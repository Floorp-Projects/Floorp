#!/usr/bin/env python3
"""Copy small QA evidence only; exclude browser state, archives and raw user metadata."""
from pathlib import Path
import json,re,shutil
src=Path(__file__).resolve().parent
dst=Path('/workspace/qa-linux-package-pwa/docs/qa/linux-22/package-pwa/full-build')
skip={'pr-info.json','pr-info-after.json','baseline-runs.json','runtime-worktree.log'}
allowed={'.json','.md','.py','.log','.txt','.png','.mn'}
files=[]
for p in sorted(src.rglob('*')):
 if not p.is_file() or p.is_symlink() or p.name in skip or p.suffix not in allowed or p.stat().st_size>1500000:continue
 rel=p.relative_to(src)
 if any(x in {'home','profile','runtime','desktop-home','__pycache__'} or x.endswith(('-home','-profile')) for x in rel.parts[:-1]):continue
 if p.suffix!='.png':
  t=p.read_text(errors='replace')
  if re.search(r'https?://[^\s"<>]+[?&](?:sig|token|se|sv)=',t):raise RuntimeError('Refusing signed URL in '+str(rel))
  if re.search(r'"(?:email|emailAddress|avatar_url|avatarUrls|login)"\s*:',t):raise RuntimeError('Refusing personal metadata in '+str(rel))
 target=dst/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 files.append({'path':str(rel),'bytes':p.stat().st_size})
(dst/'published-files.json').write_text(json.dumps(files,indent=2)+'\n')
print(json.dumps({'files':len(files),'bytes':sum(f['bytes'] for f in files)}))
