#!/usr/bin/env python3
from pathlib import Path
import sys,json,hashlib,shutil
B=Path('/workspace/qa-linux-22/package-pwa');sys.path.insert(0,str(B/'python'));from PIL import Image,ImageChops,ImageStat
D=Path(__file__).resolve().parent;record=json.loads((D/'cold-result.json').read_text());site=Path(record['desktop']['Icon']);floorp=Path('/workspace/qa-linux-additional/KAN-24/followup/products/baseline/floorp/browser/chrome/icons/default/default32.png');shot=Image.open(D/'vrtmax-cold-taskbar.png').convert('RGB');refs={'vrtmax':Image.open(site).convert('RGBA'),'floorp':Image.open(floorp).convert('RGBA')};rows={}
for label,ref in refs.items():
 candidates=[]
 for filter in ['NEAREST','BOX','BILINEAR','HAMMING','BICUBIC','LANCZOS']:
  resized=ref.resize((32,32),getattr(Image.Resampling,filter))
  for y in range(718,727):
   for x in range(258,316):
    crop=shot.crop((x,y,x+32,y+32));background=Image.new('RGBA',(32,32),(*crop.getpixel((0,0)),255));expected=Image.alpha_composite(background,resized).convert('RGB');diff=ImageChops.difference(crop,expected);score=sum(ImageStat.Stat(diff).mean)/3;candidates.append((score,x,y,filter,diff.getextrema()))
 best=min(candidates);score,x,y,filter,extrema=best;rows[label]={'meanAbsoluteChannelDifference':score,'bbox':[x,y,x+31,y+31],'resample':filter,'maxChannelDifference':max(p[1] for p in extrema),'referenceSha256':hashlib.sha256((site if label=='vrtmax' else floorp).read_bytes()).hexdigest(),'referenceDimensions':ref.size,'alphaExtrema':ref.getchannel('A').getextrema()}
bbox=rows['vrtmax']['bbox'];shot.crop((bbox[0],bbox[1],bbox[2]+1,bbox[3]+1)).save(D/'actual-vrtmax-task-icon.png');shutil.copyfile(site,D/'generated-vrtmax-icon.png');shutil.copyfile(floorp,D/'reference-floorp32.png')
fixed=shot.crop((bbox[0],bbox[1],bbox[2]+1,bbox[3]+1));fixedScores={};bg=shot.getpixel((bbox[0]-3,bbox[1]+16))
for label,ref in refs.items():
 scaled=ref.resize((32,32),Image.Resampling.BOX);expected=Image.alpha_composite(Image.new('RGBA',(32,32),(*bg,255)),scaled).convert('RGB');fixedScores[label]=sum(ImageStat.Stat(ImageChops.difference(fixed,expected)).mean)/3
result={'sameActualIconCropMeanAbsoluteDifference':fixedScores,'outsideIconTaskBackgroundRGB':bg,'scope':'rendered taskbar32x32 compared to generated real-site128pxPNG and knownFloorp32; smooth scaling tried, no claim allpixelsidentical unlessdifference0','comparisons':rows,'siteCloserThanFloorp':rows['vrtmax']['meanAbsoluteChannelDifference']<rows['floorp']['meanAbsoluteChannelDifference'],'visualReview':'actual rendered taskbar visibly VRTMAX logo; no Floorp logo'};(D/'pixel-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
