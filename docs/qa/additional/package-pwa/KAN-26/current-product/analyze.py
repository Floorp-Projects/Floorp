#!/usr/bin/env python3
"""Count actual colored tab pixels in X11 chrome captures, alongside style evidence.
Threshold counts identify blue/orange hue within each real tab box; they are not
an exact screenshot reference comparison or a claim of arbitrary theme support.
"""
import json,argparse,math,re
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args();rows=[]
for path in sorted(a.directory.glob('*.json')):
 image=path.with_suffix('.png')
 if not image.exists()or not path.name.startswith(('proton-','lepton-','protonfix-','fluerial-')):continue
 d=json.loads(path.read_text());im=Image.open(image).convert('RGB');tabs=[];sx=d.get('screen',{}).get('x',0);sy=d.get('screen',{}).get('y',0)
 for t in d['tabs']:
  r=dict(t['tab']['rect']);r['x']+=sx;r['y']+=sy;box=(max(0,math.floor(r['x'])),max(0,math.floor(r['y'])),min(im.width,math.ceil(r['x']+r['width'])),min(im.height,math.ceil(r['y']+r['height'])))
  crop=im.crop(box);px=list(crop.getdata());blue=sum(b>r+70 and g>r+30 and b>g+35 for r,g,b in px);orange=sum(r>200 and 80<g<220 and b<120 and r>b+100 for r,g,b in px)
  crop.save(a.directory/(path.stem+'-tab-'+t['role']+'.png'))
  before=t['parts']['.tab-content']['before'];line=t['parts']['.tab-context-line']['normal']
  def hue_roi(rect):
   rect=dict(rect);rect['x']+=sx;rect['y']+=sy
   if rect['width']<=0 or rect['height']<=0:return {'rect':rect,'blueHuePixels':0,'orangeHuePixels':0,'zeroGeometry':True}
   outer=(max(0,math.floor(rect['x'])),max(0,math.floor(rect['y'])),min(im.width,math.ceil(rect['x']+rect['width'])),min(im.height,math.ceil(rect['y']+rect['height'])))
   # Sample only pixel centers inside the actual fractional element rectangle.
   # floor/ceil bounding boxes can include an adjacent multiselect outline.
   bb=(max(0,math.ceil(rect['x']-.5)),max(0,math.ceil(rect['y']-.5)),min(im.width,math.ceil(rect['x']+rect['width']-.5)),min(im.height,math.ceil(rect['y']+rect['height']-.5)))
   pixels=list(im.crop(bb).getdata());outer_pixels=list(im.crop(outer).getdata())
   return {'rect':rect,'pixelBox':bb,'sampling':'pixel-centers-inside-element','blueHuePixels':sum(b>r+70 and g>r+30 and b>g+35 for r,g,b in pixels),'orangeHuePixels':sum(r>200 and 80<g<220 and b<120 and r>b+100 for r,g,b in pixels),'outerBoundingBox':outer,'outerBlueHuePixels':sum(b>r+70 and g>r+30 and b>g+35 for r,g,b in outer_pixels),'outerOrangeHuePixels':sum(r>200 and 80<g<220 and b<120 and r>b+100 for r,g,b in outer_pixels),'zeroGeometry':False}
  native=hue_roi(line['rect'])
  alternative=None
  if d['design'] in ['lepton','protonfix']:
   owner=t['parts']['.tab-content']['normal']['rect'];st=before['styles']
   def pxv(x):
    try:return float(x.removesuffix('px'))
    except ValueError:return 0
   transform=re.match(r'matrix\(1, 0, 0, 1, ([^,]+), ([^)]+)\)',st['transform']);tx,ty=map(float,transform.groups()) if transform else (0,0)
   # This indicator's border is at the bottom of a zero-height absolute pseudo.
   alternative=hue_roi({'x':owner['x']+pxv(st['left'])+tx,'y':owner['y']+pxv(st['top'])+ty+pxv(st['height']),'width':pxv(st['width']),'height':pxv(st['borderBottomWidth'])})
  tabs.append({'role':t['role'],'selected':t['tab']['attrs'].get('selected') is not None,'multiselected':t['tab']['attrs'].get('multiselected') is not None,'hover':t['hover'],'nativeLinePixels':native,'alternativeBorderPixels':alternative,'cropBox':box,'blueHuePixels':blue,'orangeHuePixels':orange,'nativeLine':{k:line['styles'][k]for k in ['display','opacity','width','height','backgroundColor']},'alternativeContentBefore':{k:before['styles'][k]for k in ['display','content','opacity','width','height','borderBottomColor','borderBottomWidth','position','bottom','left']},'tokens':before['tokens'],'crop':path.stem+'-tab-'+t['role']+'.png'})
 rows.append({'id':path.stem,'design':d['design'],'verticalMode':d['verticalMode'],'tabs':tabs})
(a.directory/'pixel-matrix.json').write_text(json.dumps(rows,indent=2))
print('cases',len(rows))
for row in rows:
 if '-selected-blue'in row['id'] or '-selected-normal'in row['id']:
  t=next(t for t in row['tabs']if t['role']=='blue');print(row['id'],'bluepixels',t['blueHuePixels'],'altwidth',t['alternativeContentBefore']['width'],'nativedisplay',t['nativeLine']['display'])
