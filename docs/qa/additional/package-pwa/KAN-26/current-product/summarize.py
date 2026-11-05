#!/usr/bin/env python3
import json
from pathlib import Path
R=Path(__file__).resolve().parent;roots={'current159':R/'run','originalVersion158':R.parent/'old-product-measurement'};rows=[]
for label,r in roots.items():
 d=json.loads((r/'pixel-matrix.json').read_text());identity=json.loads((r/'identity.json').read_text());result=json.loads((r/'result.json').read_text());assert result['completed'] and result['productUnchanged'] and result['ownedProcessesRemaining']==0
 for case in d:
  for t in case['tabs']:
   if t['role']=='normal':continue
   roi=t['alternativeBorderPixels'] if case['design']in['lepton','protonfix']else t['nativeLinePixels'];hue='blueHuePixels'if t['role']=='blue'else'orangeHuePixels'
   rows.append({'label':label,'nativeVersion':identity['version'],'buildID':identity['buildID'],'case':case['id'],'design':case['design'],'verticalMode':case['verticalMode'],'role':t['role'],'selected':t['selected'],'multiselected':t['multiselected'],'hover':t['hover'],'indicatorWidth':roi['rect']['width'],'indicatorHeight':roi['rect']['height'],'indicatorHuePixels':roi[hue]})
(R/'version-comparison.json').write_text(json.dumps(rows,indent=2))
for label in roots:
 print(label,[(x['case'],x['indicatorHuePixels'])for x in rows if x['label']==label and x['role']=='blue'and x['selected']and not x['multiselected']])
