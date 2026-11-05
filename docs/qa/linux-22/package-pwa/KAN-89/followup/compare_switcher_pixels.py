#!/usr/bin/env python3
"""Numerical comparison of the observed first Alt+Tab tile, not a classifier.
Exact PNG identity requires a direct _NET_WM_ICON pixel match; absent properties
cannot establish that. Fixed crop is checked visually against saved full images.
"""
from pathlib import Path
import sys,json,hashlib
B=Path(__file__).resolve().parent;sys.path.insert(0,str(B.parents[1]/'python'))
from PIL import Image,ImageChops,ImageStat
# First tile's 54x54 icon area in this 1280x900 two-window KWin switcher layout.
rect=(468,489,522,543)
refs={name:Image.open(B/(name+'-reference64.png')).convert('RGBA') for name in ['floorp','firefox']}
rows=[]
for locale in ['en-US','ja','es-CL']:
 path=B/(locale+'-alt-tab.png')
 if not path.exists():continue
 image=Image.open(path).convert('RGB');crop=image.crop(rect);crop.save(B/(locale+'-switcher-icon-crop.png'));background=crop.getpixel((0,0));comparisons={}
 for name,reference in refs.items():
  template=Image.new('RGBA',(54,54),background+(255,));small=reference.resize((48,48),Image.Resampling.LANCZOS);template.alpha_composite(small,(3,3));template=template.convert('RGB')
  comparisons[name]={'exactRGBMatchToDefinedRendering':crop.tobytes()==template.tobytes(),'meanAbsoluteChannelDifference':sum(ImageStat.Stat(ImageChops.difference(crop,template)).mean)/3}
 rows.append({'locale':locale,'cropRect':rect,'cropRGBSha256':hashlib.sha256(crop.tobytes()).hexdigest(),'referenceComparisons':comparisons,'interpretation':'reference render mismatch is numerical evidence only; scaling, background and cache may differ. Full screenshot visually shows generic X symbol, not an exact official PNG identity claim.'})
taskbar=[]
reference32=Image.open(B/'floorp-reference32.png').convert('RGBA')
for locale in ['en-US','ja','es-CL']:
 image=Image.open(B/(locale+'-alt-tab.png')).convert('RGB');crop=image.crop((222,862,254,894));crop.save(B/(locale+'-taskbar-icon-crop.png'))
 background=crop.getpixel((0,0));template=Image.new('RGBA',(32,32),background+(255,));template.alpha_composite(reference32);template=template.convert('RGB');difference=ImageChops.difference(crop,template)
 taskbar.append({'locale':locale,'cropRect':[222,862,254,894],'exactRGBMatchToFloorpDefault32OnSampledBackground':crop.tobytes()==template.tobytes(),'meanAbsoluteChannelDifference':sum(ImageStat.Stat(difference).mean)/3,'maximumChannelDifference':max(e[1] for e in difference.getextrema()),'note':'taskbar surface differs from KWin AltTab tile; small alpha compositing differences are not exact PNG identity'})
(B/'taskbar-pixel-comparison.json').write_text(json.dumps(taskbar,indent=2))
result={'taskbarRows':taskbar,'rows':rows,'allLocaleCropsExactlyIdentical':len(rows)==3 and len({r['cropRGBSha256'] for r in rows})==1,'directPropertyPixelComparison':'not possible: actual client _NET_WM_ICON absent in all three runs','input':'synthetic Alt+Tab'}
(B/'pixel-comparison.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
