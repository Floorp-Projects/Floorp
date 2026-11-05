#!/usr/bin/env python3
"""Separate container-line side observation. Does not grade KAN95 margins."""
import argparse
import json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
b={x['id']:x for x in json.loads((a.baseline/'pixel-matrix.json').read_text())}
c={x['id']:x for x in json.loads((a.candidate/'pixel-matrix.json').read_text())}
assert len(b)==len(c)==24 and set(b)==set(c)
rows=[]
for case,old in b.items():
    new=c[case];bt={t['role']:t for t in old['tabs']};ct={t['role']:t for t in new['tabs']}
    for role in ['blue','orange']:
        key='alternativeBorderPixels'if old['design']=='lepton'else'nativeLinePixels'
        hue=role+'HuePixels';other=('orange'if role=='blue'else'blue')+'HuePixels'
        before=bt[role][key];after=ct[role][key]
        visible_before=before[hue]>0;visible_after=after[hue]>0
        rows.append({'case':case,'role':role,'source':key,'rectBefore':before['rect'],'rectAfter':after['rect'],'correctBefore':before[hue],'correctAfter':after[hue],'oppositeBefore':before[other],'oppositeAfter':after[other],'newLoss':visible_before and not visible_after,'newWrongHue':before[other]==0 and after[other]>0,'classification':'preexisting-missing-indicator'if not visible_before and not visible_after else'visible-preserved'if visible_before and visible_after else'new-loss'if visible_before else'newly-visible'})
result={'key':'KAN-95','separateFromMarginQualification':True,'prior3041And3048Included':False,'stateCount':24,'indicatorCount':48,'newLosses':sum(x['newLoss']for x in rows),'newWrongHue':sum(x['newWrongHue']for x in rows),'preexistingMissing':sum(x['classification']=='preexisting-missing-indicator'for x in rows),'rows':rows,'scope':'selected blue, inactive orange, no hover/multiselection; pixel centers inside actual indicator, synthetic UI'}
a.out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k]for k in ['newLosses','newWrongHue','preexistingMissing']}))
