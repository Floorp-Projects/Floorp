#!/usr/bin/env python3
"""Authenticate Ubuntu CI ZIP and retain target successful execution evidence."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zip',type=Path,required=True);p.add_argument('--metadata',type=Path,required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;meta=json.loads(a.metadata.read_text());art=meta['artifact'];run=meta['run'];head='0e80f14dc439dde7817c0620f0fc112d54dfba80'
assert run['head_sha']==head and run['id']==42112961086
assert art['workflow_run']['head_sha']==head and art['workflow_run']['id']==run['id'] and not art['expired']
sha=hashlib.file_digest(a.zip.open('rb'),'sha256').hexdigest();assert art['digest']=='sha256:'+sha;assert a.zip.stat().st_size==art['size_in_bytes']
TARGETS=['lepton-container-geometry.test.ts','fluerial-container.test.ts'];CONTROLS=['container-indicator.test.ts','fluerial-groups.test.ts','lepton-compat.test.ts'];excerpts=[];checks={n:False for n in TARGETS};controls={n:False for n in CONTROLS};summary=None;downloaded=None
with zipfile.ZipFile(a.zip) as archive:
 for name in archive.namelist():
  if not name.endswith('.log'):continue
  content=archive.read(name).decode('utf8',errors='replace');lines=content.splitlines()
  if 'ci-combined-browser-test.log' in name or name.endswith('logs/test/latest.log'):
   dest=R/'evidence'/name.replace('/','__');dest.parent.mkdir(exist_ok=True);dest.write_text(content)
  for i,line in enumerate(lines):
   target=next((n for n in TARGETS if n in line),None);control=next((n for n in CONTROLS if n in line),None)
   if target or control or any(s in line for s in ['Browser test result:','Downloaded Firefox coverage:','[META] exitCode=','appBuildID','Services.version']):
    excerpts.append({'file':name,'line':i+1,'text':'\n'.join(lines[max(0,i-1):i+3])})
   if target and '✓' in line and 'runAllTests' in line:checks[target]=True
   if control and '✓' in line and 'runAllTests' in line:controls[control]=True
   if 'ci-combined-browser-test.log' in name:
    m=re.search(r'Browser test result: (\d+) passed, (\d+) failed',line)
    if m:summary={'modulesPassed':int(m[1]),'modulesFailed':int(m[2])}
    m=re.search(r'Downloaded Firefox coverage: (\d+) file\(s\), (\d+) task\(s\), (\d+) passed',line)
    if m:downloaded={'files':int(m[1]),'tasks':int(m[2]),'tasksPassed':int(m[3])}
result={'run':run['id'],'headSha':head,'artifactId':art['id'],'artifactName':art['name'],'apiDigest':art['digest'],'actualSha256':sha,'digestVerified':True,'targetModulesSuccessful':checks,'existingControlModulesSuccessful':controls,'summary':summary,'downloadedFirefox':downloaded,'excerpts':excerpts,'qualification':'standard repository development harness; independent of sandbox6 complete production product verification','declaredCasesPerModule':2,'perCaseSuccessLinesClaimed':False}
(R/'artifact-verification.json').write_text(json.dumps(result,indent=2)+'\n')
assert summary and summary['modulesFailed']==0 and all(checks.values()) and all(controls.values()),'Standard browser suite or target modules not successful; retain evidence'
print(json.dumps({k:result[k] for k in ['artifactId','digestVerified','targetModulesSuccessful','summary','downloadedFirefox']}))
