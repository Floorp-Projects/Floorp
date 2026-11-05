#!/usr/bin/env python3
"""Strict natural-style KAN95 full-product comparison, no UI execution/injection."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path, required=True)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--baseline-intake', type=Path, required=True)
parser.add_argument('--candidate-intake', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
a = parser.parse_args()
criteria = json.loads((Path(__file__).parent/'criteria.json').read_text())
checks = []
rows = []

def read(path):
    return json.loads(path.read_text())

def check(name, value, detail=None):
    row = {'name': name, 'pass': bool(value)}
    if detail is not None:
        row['detail'] = detail
    checks.append(row)

def near(x, y):
    return abs(float(x)-float(y)) <= 0.1

def guard(root, intake_path, expected_sha, candidate):
    result = read(root/'result.json')
    identity = read(root/'identity.json')
    intake = read(intake_path)
    check(str(root)+':completed', result.get('completed') is True and 'error' not in result)
    check(str(root)+':source', result.get('sourceSha') == expected_sha == intake['sourceSha'])
    check(str(root)+':sandbox', identity['sandbox'] == 6 and identity['disabled'] is False)
    check(str(root)+':native', identity['version'] == '159.0.1' and identity['buildID'] == '20261103153232')
    product = Path(result['product']).resolve()
    check(str(root)+':executable', Path(identity['executable']).resolve() == (product/'floorp').resolve())
    check(str(root)+':profile', Path(identity['profile']).resolve() == (root/'profile').resolve())
    check(str(root)+':normalquit', result.get('browserExit') == 0 and result.get('quit',{}).get('forced') is False and result.get('quit',{}).get('in_app') is True)
    check(str(root)+':ownedprocess0', result.get('ownedProcessesRemaining') == 0 and result.get('displaySocketAbsent') is True)
    before = read(root/'product-before.json');after = read(root/'product-after.json')
    bt = read(root/'product-tree-before.json');at = read(root/'product-tree-after.json')
    check(str(root)+':filesunchanged', result.get('productUnchanged') is True and before == after)
    check(str(root)+':treeunchanged', result.get('productTreeUnchanged') is True and bt == at)
    inventory = read(intake_path.parent/intake.get('productInventoryFile','product-inventory-before.json'))
    expected = {x['path']:x['sha256'] for x in inventory if 'sha256' in x} if isinstance(inventory,list) else {k:v for k,v in inventory.items() if isinstance(v,str)}
    check(str(root)+':authenticatedfiles', before == expected)
    check(str(root)+':successfulintake', intake['run']['conclusion'] == 'success' and intake['run']['head_sha'] == expected_sha)
    check(str(root)+':artifactbinding', intake['artifact']['workflow_run']['head_sha'] == expected_sha and intake['artifact']['expired'] is False and intake['artifact']['digest'] == 'sha256:'+intake['archiveZipSha256'])
    if candidate:
        check('candidate-exact-package-run', intake['run']['id'] == intake['artifact']['workflow_run']['id'] == criteria['candidatePackageRun'])
    return {x['id']:read(root/(x['id']+'.json')) for x in result['cases']}

try:
    baseline = guard(a.baseline,a.baseline_intake,criteria['baselineSource'],False)
    candidate = guard(a.candidate,a.candidate_intake,criteria['candidateSource'],True)
    check('24unique-baseline',len(baseline)==24)
    check('24unique-candidate',len(candidate)==24)
    check('matching24cases',set(baseline)==set(candidate))
    expected_ids={design+'-'+('v'if vertical else'h')+'-density-'+str(density) for design in criteria['designs'] for vertical in [False,True] for density in [0,1,2]}
    check('all-planned24cases',set(baseline)==set(candidate)==expected_ids)
    for case,b in baseline.items():
        c=candidate[case]
        check(case+':actual-state',b['design']==c['design'] and b['density']==c['density'] and b['native']==b['verticalMode']==c['native']==c['verticalMode'])
        check(case+':named-state-is-actual',all(case==x['design']+'-'+('v'if x['verticalMode']else'h')+'-density-'+str(x['density']) for x in [b,c]))
        check(case+':effective-gates-match',b['effectivePrefs']==c['effectivePrefs'])
        if c['design']=='lepton':check(case+':lepton-padding-enabled',c['effectivePrefs']['userChrome.tab.lepton_like_padding'] is True)
        check(case+':defaulttheme',b['rootAttrs'].get('theme-effective-id')==c['rootAttrs'].get('theme-effective-id')=='default-theme@mozilla.org')
        check(case+':native-orientation',c['tabContainerAttrs']['orient']==('vertical'if c['verticalMode'] else'horizontal'))
        check(case+':density',c['density']in[0,1,2])
        check(case+':actual-root-density',c['rootAttrs'].get('uidensity')==[None,'compact','touch'][c['density']] and b['rootAttrs'].get('uidensity')==[None,'compact','touch'][b['density']])
        check(case+':fonts-ready',b['fontsStatus']==c['fontsStatus']=='loaded')
        change=c['design']=='lepton' and not c['verticalMode'] and c['density']in[0,1]
        check(case+':legacy-token-preserved',b['legacyRootToken']==c['legacyRootToken'])
        if c['design']=='lepton'and not c['verticalMode']:
            expected='4px'if c['density']==2 else'2px'
            check(case+':native-token-consumes-legacy',c['nativeRootToken'].strip()==c['legacyRootToken'].strip()==expected)
        else:
            check(case+':native-token-unchanged',b['nativeRootToken']==c['nativeRootToken'])
        br={t['role']:t for t in b['tabs']};cr={t['role']:t for t in c['tabs']}
        check(case+':realroles',set(br)==set(cr)=={'blue','orange','normal'})
        for role,t in cr.items():
            old=br[role];bg=t['parts']['.tab-background']['normal'];obg=old['parts']['.tab-background']['normal'];stack=t['parts']['.tab-stack']['normal']['rect'];rect=bg['rect']
            old_stack=old['parts']['.tab-stack']['normal']['rect'];old_rect=obg['rect']
            start=rect['y']-stack['y'];old_start=old_rect['y']-old_stack['y']
            end=stack['y']+stack['height']-rect['y']-rect['height'];old_end=old_stack['y']+old_stack['height']-old_rect['y']-old_rect['height']
            check(case+':'+role+':selection',('selected'in t['tab']['attrs'])==(role=='blue') and ('selected'in old['tab']['attrs'])==(role=='blue'))
            check(case+':'+role+':actual-no-hover-or-multiselect',not t['hover'] and not old['hover'] and 'multiselected'not in t['tab']['attrs'] and 'multiselected'not in old['tab']['attrs'])
            if role!='normal':
                check(case+':'+role+':genuinecontainer',int(t['tab']['attrs'].get('usercontextid','0'))>0 and'identity-color-'+role in t['tab']['attrs'].get('class',''))
            for prop in ['marginBlockStart','marginBlockEnd']:
                expected='2px'if change and prop=='marginBlockStart' else obg['styles'][prop]
                check(case+':'+role+':'+prop,bg['styles'][prop]==expected,{'before':obg['styles'][prop],'after':bg['styles'][prop],'expected':expected})
                if change and prop=='marginBlockStart':check(case+':'+role+':baseline-regression-'+prop,obg['styles'][prop]=='4px')
            check(case+':'+role+':positive-area',rect['height']>0 and rect['width']>0)
            check(case+':'+role+':paint-gap-start',near(old_start,4) and near(start,2) if change else near(start,old_start),{'before':old_start,'after':start})
            if not change:
                check(case+':'+role+':paint-gap-end-unchanged',near(end,old_end),{'before':old_end,'after':end})
            if not change:
                check(case+':'+role+':height-unchanged',near(rect['height'],obg['rect']['height']))
            rows.append({'case':case,'role':role,'intentionalChange':change,'marginBefore':obg['styles']['marginBlockStart'],'marginAfter':bg['styles']['marginBlockStart'],'gapStartBefore':old_start,'gapStart':start,'gapEndBefore':old_end,'gapEnd':end,'backgroundHeightBefore':old_rect['height'],'backgroundHeight':rect['height']})
except Exception as error:
    check('comparison-complete-evidence',False,{'error':str(error),'type':type(error).__name__})
result={'key':'KAN-95','sourceSha':criteria['candidateSource'],'qualified':all(x['pass']for x in checks),'passed':sum(x['pass']for x in checks),'failed':sum(not x['pass']for x in checks),'checks':checks,'rows':rows,'scope':'Debian/Xvfb realnative tabs; no stylesheet injection; not originalWindows10/hardware qualification'}
a.out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k]for k in ['qualified','passed','failed']}))
raise SystemExit(0 if result['qualified']else 1)
