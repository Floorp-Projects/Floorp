#!/usr/bin/env python3
"""Inventory standard before-mach output without modifying it."""
from pathlib import Path
import json,hashlib,subprocess,re,shutil,os
OUT=Path(__file__).resolve().parent;WORK=Path('/workspace/qa-linux-full-build/floorp3030');ROOT=WORK/'_dist/noraneko';record=json.loads((OUT/'build-record.json').read_text());assert record.get('frontendBeforeMachCompleted')
source=subprocess.check_output(['git','-C',str(WORK),'rev-parse','HEAD'],text=True).strip();assert source==record['sourceSha']
expected=re.findall(r'"_dist/noraneko/([^"/]+)"',(WORK/'moz.build').read_text());assert len(expected)==11,expected
entries=[]
for current,dirs,files in os.walk(ROOT,followlinks=True):
 for name in sorted(files):
  p=Path(current)/name
  if p.is_file():
   assert p.resolve().is_relative_to(WORK),str(p)
   entries.append({'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
entries.sort(key=lambda row:row['path'])
encoded=json.dumps(entries,sort_keys=True,separators=(',',':')).encode();inventory={'sourceSha':source,'treeSha':subprocess.check_output(['git','-C',str(WORK),'rev-parse','HEAD^{tree}'],text=True).strip(),'scope':'all regular files in standard _dist/noraneko output following standard component directory symlinks into this worktree; not native/package files','entries':entries,'fileCount':len(entries),'totalBytes':sum(e['bytes'] for e in entries),'manifestListSha256':hashlib.sha256(encoded).hexdigest()}
(OUT/'frontend-files.json').write_text(json.dumps(inventory,indent=2))
components=[]
for name in expected:
 d=ROOT/name;assert d.is_dir(),name;assert (d/'moz.build').is_file(),name;assert (d/'jar.mn').is_file(),name
 local=[e for e in entries if e['path'].startswith(name+'/')]
 components.append({'name':name,'fileCount':len(local),'bytes':sum(e['bytes'] for e in local),'jarSha256':hashlib.sha256((d/'jar.mn').read_bytes()).hexdigest(),'mozBuildSha256':hashlib.sha256((d/'moz.build').read_bytes()).hexdigest()})
jar=(ROOT/'content/jar.mn').read_text();overrides=[line for line in jar.splitlines() if line.startswith('% override ')]
assert overrides==['% override chrome://branding/locale/brand.properties resource:///chrome/en-US/locale/branding/brand.properties'],overrides
shutil.copyfile(ROOT/'content/jar.mn',OUT/'generated-content-jar.mn')
summary={'sourceSha':source,'sourceTreeSha':inventory['treeSha'],'runtimeCommit':record['runtimeCommit'],'runtimeLockSha256':record['runtimeLockSha256'],'denoLockSha256':hashlib.sha256((WORK/'deno.lock').read_bytes()).hexdigest(),'buildid2':(WORK/'_dist/buildid2').read_text().strip(),'componentLinks':{name:str((ROOT/name).resolve().relative_to(WORK)) for name in expected},'components':components,'fileCount':inventory['fileCount'],'totalBytes':inventory['totalBytes'],'frontendManifestListSha256':inventory['manifestListSha256'],'contentOverrides':overrides,'trackedStatusAfter':subprocess.check_output(['git','-C',str(WORK),'status','--porcelain'],text=True).strip(),'frontendBeforeMachCompleted':True,'fullProductCompleted':False,'remainingStages':['locked Runtime configure','mach build','normal product packaging','full product execution/locale comparison']}
assert summary['trackedStatusAfter']=='',summary['trackedStatusAfter'];(OUT/'frontend-summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
