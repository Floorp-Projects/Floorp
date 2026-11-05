#!/usr/bin/env python3
"""Prepare an isolated product copy with only the built Chrome URI override.

Usage: prepare_candidate.py ORIGINAL_PRODUCT COPY_DESTINATION GENERATED_JAR_MN
The original directory is never modified. The destination must not exist.
"""
import argparse
from pathlib import Path
import shutil
import zipfile
import hashlib
import json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('original',type=Path);p.add_argument('destination',type=Path);p.add_argument('jar_mn',type=Path)
a=p.parse_args()
original=a.original.resolve(strict=True);destination=a.destination.resolve()
assert not destination.exists(),'destination must be new'
assert destination!=original and original not in destination.parents,'copy must not be inside original'
lines=[line for line in a.jar_mn.read_text().splitlines() if line.startswith('% override ')]
expected='% override chrome://branding/locale/brand.properties resource:///chrome/en-US/locale/branding/brand.properties'
assert lines==[expected],lines
shutil.copytree(original,destination)
archive=destination/'browser/omni.ja';temporary=archive.with_suffix('.qa-new')
with zipfile.ZipFile(archive) as source,zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED) as out:
 for entry in source.infolist():
  data=source.read(entry.filename)
  if entry.filename=='chrome/chrome.manifest':
   assert expected.removeprefix('% ').encode() not in data,'already patched'
   data+=('\n'+expected.removeprefix('% ')+'\n').encode()
  out.writestr(entry,data)
temporary.replace(archive)
print(json.dumps({'original':str(original),'destination':str(destination),'generatedJarSha256':hashlib.sha256(a.jar_mn.read_bytes()).hexdigest(),'registration':expected.removeprefix('% '),'logicalChangedMember':'browser/omni.ja:chrome/chrome.manifest','note':'isolated manually repacked product integration fixture; not full source build'},indent=2))
