#!/usr/bin/env python3
"""Qualify a complete KAN26 candidate against the recorded 88-state baseline.

No browser execution, injected styles or result rewriting. This validates actual
state/indicator ROIs, controls, complete-product identity and strict tree guards.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--candidate',type=Path,required=True)
p.add_argument('--product',type=Path,required=True)
p.add_argument('--intake',type=Path,required=True)
p.add_argument('--baseline',type=Path,default=Path(__file__).parent/'baseline-88-pixel-matrix.json')
p.add_argument('--expected-sha',default='0e80f14dc439dde7817c0620f0fc112d54dfba80')
p.add_argument('--expected-run',type=int,default=42113006050)
p.add_argument('--expected-artifact',type=int)
p.add_argument('--out',type=Path)
a=p.parse_args();root=a.candidate.resolve();product=a.product.resolve(strict=True)
output=a.out or root/'comparison.json';checks=[];style_differences=[]
def read(path):return json.loads(Path(path).read_text())
def check(name,passed,details=None):
 row={'name':name,'pass':bool(passed)}
 if details is not None:row['details']=details
 checks.append(row)
def near(x,y):return isinstance(x,(float,int)) and isinstance(y,(float,int)) and abs(x-y)<=0.1

def tree_snapshot():
 result={}
 for item in sorted(product.rglob('*')):
  key=str(item.relative_to(product))
  if item.is_symlink():result[key]={'kind':'symlink','target':str(item.readlink())}
  elif item.is_dir():result[key]={'kind':'directory'}
  elif item.is_file():
   with item.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
   result[key]={'kind':'file','sha256':digest,'size':item.stat().st_size}
 return result

def compare_visual(case,role,before,after):
 """Only painted/layout styles, not intentionally repaired inherited aliases."""
 parts=['tab','before','after']
 for part in parts:
  b=before[part];c=after[part]
  if b is None or c is None:
   if b!=c:style_differences.append({'case':case,'role':role,'part':part,'reason':'element-presence'})
   continue
  for prop,value in b['styles'].items():
   if c['styles'].get(prop)!=value:style_differences.append({'case':case,'role':role,'part':part,'property':prop,'before':value,'after':c['styles'].get(prop)})
 for selector,bparts in before['parts'].items():
  cparts=after['parts'].get(selector,{})
  for pseudo,b in bparts.items():
   c=cparts.get(pseudo)
   if b is None or c is None:
    if b!=c:style_differences.append({'case':case,'role':role,'part':selector+' '+pseudo,'reason':'element-presence'})
    continue
   for prop,value in b['styles'].items():
    if c['styles'].get(prop)!=value:style_differences.append({'case':case,'role':role,'part':selector+' '+pseudo,'property':prop,'before':value,'after':c['styles'].get(prop)})
   # Compare dimensions and relative positions, independent of X11 frame origin.
   for dimension in ['width','height']:
    if not near(b['rect'][dimension],c['rect'][dimension]):style_differences.append({'case':case,'role':role,'part':selector+' '+pseudo,'property':'rect.'+dimension,'before':b['rect'][dimension],'after':c['rect'][dimension]})
   for dimension in ['x','y']:
    bv=b['rect'][dimension]-before['tab']['rect'][dimension];cv=c['rect'][dimension]-after['tab']['rect'][dimension]
    if not near(bv,cv):style_differences.append({'case':case,'role':role,'part':selector+' '+pseudo,'property':'relative.'+dimension,'before':bv,'after':cv})

try:
 result=read(root/'result.json');identity=read(root/'identity.json');intake=read(a.intake)
 check('exact-source',result.get('sourceSha')==a.expected_sha)
 check('all-states-mode',result.get('requestedStateMode')=='all-states')
 check('run-completed',result.get('completed') is True and 'error' not in result)
 check('native-version',identity.get('version')=='159.0.1')
 check('native-build-id',identity.get('buildID')=='20261103153232')
 check('content-sandbox-preserved',identity.get('sandbox')==6 and identity.get('disabled') is False)
 check('actual-product-executable',Path(identity.get('executable','/missing')).resolve()==(product/'floorp').resolve())
 check('actual-product-argument',Path(result.get('product','/missing')).resolve()==product)
 check('owned-profile',Path(identity.get('profile','/missing')).resolve()==root/'profile')
 check('containers-enabled',identity.get('containers') is True)
 check('normal-in-app-quit',result.get('browserExit')==0 and result.get('quit',{}).get('forced') is False and result.get('quit',{}).get('in_app') is True)
 check('owned-processes-zero',result.get('ownedProcessesRemaining')==0)
 check('owned-display-clean',result.get('displaySocketAbsent') is True)
 before_files=read(root/'product-before.json');after_files=read(root/'product-after.json')
 before_tree=read(root/'product-tree-before.json');after_tree=read(root/'product-tree-after.json')
 check('recorded-file-hashes-unchanged',result.get('productUnchanged') is True and before_files==after_files)
 mutations=[k for k in sorted(set(before_tree)|set(after_tree)) if before_tree.get(k)!=after_tree.get(k)]
 check('recorded-complete-tree-unchanged',result.get('productTreeUnchanged') is True and not mutations,mutations)
 actual=tree_snapshot();changes=[k for k in sorted(set(after_tree)|set(actual)) if after_tree.get(k)!=actual.get(k)]
 check('current-complete-tree-still-matches',not changes,changes)
 check('intake-source',intake.get('sourceSha')==a.expected_sha)
 run=intake.get('run',{});artifact=intake.get('artifact',{})
 check('intake-successful-exact-run',run.get('id')==a.expected_run and run.get('head_sha')==a.expected_sha and run.get('status')=='completed' and run.get('conclusion')=='success')
 check('intake-artifact-bound-to-run',artifact.get('workflow_run',{}).get('id')==a.expected_run and artifact.get('workflow_run',{}).get('head_sha')==a.expected_sha and artifact.get('expired') is False)
 check('intake-artifact-digest',artifact.get('digest')=='sha256:'+intake.get('archiveZipSha256',''))
 if a.expected_artifact is not None:check('intake-expected-artifact',artifact.get('id')==a.expected_artifact)
 inventory_path=a.intake.parent/intake.get('productInventoryFile','product-inventory-before.json')
 inventory=read(inventory_path)
 if isinstance(inventory,list):
  expected_files={e['path']:e['sha256'] for e in inventory if 'sha256' in e};expected_links={e['path']:e['symlink']for e in inventory if 'symlink'in e}
 elif isinstance(inventory,dict):
  expected_files={k:v for k,v in inventory.items() if isinstance(v,str)};expected_links={k:v['symlink']for k,v in inventory.items()if isinstance(v,dict)and'symlink'in v}
 else:raise RuntimeError('Unsupported authenticated inventory schema')
 actual_files={k:v['sha256']for k,v in before_tree.items()if v['kind']=='file'};actual_links={k:v['target']for k,v in before_tree.items()if v['kind']=='symlink'}
 check('candidate-files-match-authenticated-intake',expected_files==actual_files)
 check('candidate-symlinks-match-authenticated-intake',expected_links==actual_links)
 baseline=read(a.baseline);candidate=read(root/'pixel-matrix.json')
 baseline_byid={x['id']:x for x in baseline};candidate_byid={x['id']:x for x in candidate}
 check('exact88unique-baseline',len(baseline)==len(baseline_byid)==88)
 check('exact88unique-candidate',len(candidate)==len(candidate_byid)==len(result.get('cases',[]))==88)
 check('matching88-case-ids',set(baseline_byid)==set(candidate_byid))
 indicator_rows=[]
 for case,b in baseline_byid.items():
  if case not in candidate_byid:continue
  c=candidate_byid[case];check(case+':actual-design-orientation',b['design']==c['design'] and b['verticalMode']==c['verticalMode'])
  raw=read(root/(case+'.json'));baseline_raw=read(Path(a.baseline).parent/b['rawStyleSnapshot']);baseline_raw_roles={x['role']:x for x in baseline_raw['tabs']};check(case+':actual-native-vertical-mode',raw.get('native')==raw.get('verticalMode')==b['verticalMode'])
  check(case+':default-theme',raw.get('rootAttrs',{}).get('theme-effective-id')=='default-theme@mozilla.org')
  prefs=raw.get('effectivePrefs',{})
  check(case+':effective-default-container-gates',prefs.get('userChrome.tab.container') is True and prefs.get('userChrome.tab.photon_like_contextline') is False and prefs.get('userChrome.tab.close_button_at_hover') is True and prefs.get('userChrome.tab.container.always_long') in [None,False] and prefs.get('userChrome.tab.container.on_top') in [None,False])
  if b['verticalMode']:check(case+':actual-expanded-vertical',raw.get('tabContainerAttrs',{}).get('orient')=='vertical' and 'expanded'in raw.get('tabContainerAttrs',{}))
  else:check(case+':actual-horizontal',raw.get('tabContainerAttrs',{}).get('orient')=='horizontal')
  bt={x['role']:x for x in b['tabs']};ct={x['role']:x for x in c['tabs']};rt={x['role']:x for x in raw['tabs']}
  check(case+':matching-real-roles',set(bt)==set(ct)==set(rt)=={'blue','orange','normal'})
  for role in ['blue','orange','normal']:
   if role not in ct or role not in rt:continue
   before=bt[role];after=ct[role]
   check(case+':'+role+':actual-interaction-flags',all(before.get(k)==after.get(k) for k in ['selected','multiselected','hover']))
   if role=='normal':
    check(case+':normal:no-identity-pixels',after['blueHuePixels']==after['orangeHuePixels']==0)
    check(case+':normal:background-position-z-stable',all(before['nativeLine'].get(k)==after['nativeLine'].get(k)for k in ['display','opacity']) and all(rt[role]['parts']['.tab-background']['normal']['styles'][k]==baseline_raw_roles[role]['parts']['.tab-background']['normal']['styles'][k]for k in ['position','zIndex']))
    compare_visual(case,role,baseline_raw_roles[role],rt[role])
    continue
   check(case+':'+role+':genuine-container',int(rt[role]['tab']['attrs'].get('usercontextid','0'))>0 and 'identity-color-'+role in rt[role]['tab']['attrs'].get('class','').split())
   roi=after['alternativeBorderPixels'] if c['design']in['lepton','protonfix']else after['nativeLinePixels']
   hue='blueHuePixels'if role=='blue'else'orangeHuePixels';other='orangeHuePixels'if role=='blue'else'blueHuePixels'
   painted=roi is not None and roi['rect']['width']>0 and roi['rect']['height']>0 and roi[hue]>0 and roi[other]==0
   check(case+':'+role+':actual-correct-identity-indicator',painted)
   indicator_rows.append({'case':case,'role':role,'source':'alternative-border'if c['design']in['lepton','protonfix']else'native-line','selected':after['selected'],'hover':after['hover'],'multiselected':after['multiselected'],'rect':roi['rect']if roi else None,'correctHuePixels':roi[hue]if roi else None,'otherHuePixels':roi[other]if roi else None,'pass':painted})
   if c['design']=='proton':
    before_roi=before['nativeLinePixels'];check(case+':'+role+':proton-roi-stable',all(near(before_roi['rect'][k],roi['rect'][k]) for k in ['width','height']) and before_roi[hue]==roi[hue] and before_roi[other]==roi[other])
    compare_visual(case,role,baseline_raw_roles[role],rt[role])
  if 'requestedInteraction'in raw:
   expected=raw['requestedInteraction'];check(case+':requested-actual-hover',ct[expected['hover']]['hover'] is True);check(case+':requested-actual-primary',ct[expected['selected']]['selected'] is True)
   check(case+':requested-actual-multiselect',all(ct[role]['multiselected']==expected['multi']for role in ['blue','orange']))
 check('all-normal-and-proton-visual-styles-stable',not style_differences,{'differenceCount':len(style_differences)})
except Exception as e:
 check('comparison-executed-with-complete-evidence',False,{'errorType':type(e).__name__,'error':str(e)})
 indicator_rows=locals().get('indicator_rows',[])
summary={'key':'KAN-26','sourceSha':a.expected_sha,'expectedRun':a.expected_run,'qualificationScope':'complete candidate nonPhoton default-gate rendering on Debian/Xvfb; not original Windows/highDPI/physicalGPU/signed release','qualified':all(x['pass']for x in checks),'checksPass':sum(x['pass']for x in checks),'checksFail':sum(not x['pass']for x in checks),'indicatorRows':indicator_rows,'checks':checks,'visualStyleDifferences':style_differences}
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:summary[k]for k in ['key','sourceSha','qualified','checksPass','checksFail']}))
if not summary['qualified']:print(json.dumps([x for x in checks if not x['pass']][:20],indent=2))
sys.exit(0 if summary['qualified']else 1)
