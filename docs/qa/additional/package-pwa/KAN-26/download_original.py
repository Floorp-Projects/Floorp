#!/usr/bin/env python3
"""Restore the exact official original-version Linux product from public release."""
from pathlib import Path
import argparse,hashlib,json,tarfile,tempfile,urllib.request,shutil
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
url='https://github.com/Floorp-Projects/Floorp/releases/download/v12.20.2/floorp-linux-x86_64.tar.xz'
expected='059a85f0e40c9f3649ae61fe716ecbdde85f06971a600aeec5c3e8f3a0af6004'
assert not a.out.exists(),'Refuse to overwrite an existing output'
a.out.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='kan26-public-archive-',dir=a.out.parent) as tmp:
 archive=Path(tmp)/'floorp.tar.xz'
 with urllib.request.urlopen(url,timeout=120) as response,archive.open('xb') as target:shutil.copyfileobj(response,target)
 actual=hashlib.file_digest(archive.open('rb'),'sha256').hexdigest();assert actual==expected and archive.stat().st_size==108466460
 with tarfile.open(archive,'r:xz') as t:
  members=t.getmembers();names=[m.name for m in members];assert len(names)==len(set(names));assert all(Path(n).parts[0]=='floorp' for n in names)
  a.out.mkdir();t.extractall(a.out,filter='data')
result={'release':'v12.20.2','asset':646221451,'archiveSha256':expected,'product':str(a.out/'floorp'),'qualification':'official original-version Linux product; not original Windows qualification'}
(a.out/'restore-intake.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
