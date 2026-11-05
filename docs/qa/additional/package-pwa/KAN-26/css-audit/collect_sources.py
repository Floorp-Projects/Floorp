#!/usr/bin/env python3
"""Read-only source evidence for KAN26 non-Photon rendering contracts."""
from pathlib import Path
import subprocess,json,hashlib
ROOT=Path('/workspace/qa-kan24-main')
RUNTIME=Path('/workspace/qa-linux-full-build/runtime')
OUT=Path(__file__).resolve().parent
SOURCES=[
(ROOT,'browser-features/chrome/common/designs/utils/css.ts',[(165,275)]),
(ROOT,'browser-features/chrome/common/designs/browser-design-element.tsx',[(125,175)]),
(ROOT,'browser-features/skin/lepton/userjs/lepton.js',[(25,53),(176,184),(245,255)]),
(ROOT,'browser-features/skin/lepton/userjs/protonfix.js',[(25,53),(176,184),(245,255)]),
(ROOT,'browser-features/skin/lepton/css/leptonChrome.css',[(9038,9118),(24523,24608)]),
(ROOT,'browser-features/skin/fluerial/css/fluerial.css',[(118,183),(213,243)]),
(ROOT,'browser-features/chrome/common/designs/utils/tab-color-like-toolbar.css.ts',[(10,110)]),
(ROOT,'browser-features/chrome/common/designs/utils/gecko-152-var-aliases.css.ts',[(70,145)]),
(ROOT,'browser-features/chrome/common/designs/utils/lepton-compat-152.css.ts',[(145,175),(258,270)]),
(RUNTIME,'browser/themes/shared/tabbrowser/tabs.css',[(354,374),(1187,1232)]),
(RUNTIME,'browser/themes/shared/tabbrowser/tab.tokens.css',[(75,85)])]
def git(root,*args):return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()
rows=[]
for repo,path,ranges in SOURCES:
 p=repo/path;raw=p.read_bytes();lines=raw.decode().splitlines()
 rows.append({'repository':'Floorp' if repo==ROOT else 'Floorp-Runtime','head':git(repo,'rev-parse','HEAD'),'path':path,'gitBlob':git(repo,'rev-parse','HEAD:'+path),'sha256':hashlib.sha256(raw).hexdigest(),'ranges':[{'start':a,'end':b,'lines':[{'line':i,'text':lines[i-1]} for i in range(a,min(b,len(lines))+1)]} for a,b in ranges]})
result={'kind':'read-only design source audit; no actual rendering claim','floorpHead':git(ROOT,'rev-parse','HEAD'),'runtimeHead':git(RUNTIME,'rev-parse','HEAD'),'worktreeTrackedChanges':git(ROOT,'status','--short'),'runtimeTrackedChanges':git(RUNTIME,'status','--short'),'sources':rows,'noProductOrSourceMutation':True}
(OUT/'source-evidence.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'sourceCount':len(rows),'floorpHead':result['floorpHead'],'runtimeHead':result['runtimeHead'],'trackedChanges':result['worktreeTrackedChanges']}))
