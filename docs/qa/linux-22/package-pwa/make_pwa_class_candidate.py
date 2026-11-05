#!/usr/bin/env python3
"""Make a disposable packaged JS candidate copy, not a full source build.
The two resource changes correspond exactly to the three-line source operation:
LinuxSupport.getWindowClass shared by launcher generation and Linux window args.
Native binaries and all other resources must remain byte-identical.
"""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

root=Path(__file__).resolve().parent
original=root/'product/floorp'
candidate=root/'product-candidate/floorp'
if candidate.exists():
    raise SystemExit('Refusing to overwrite an existing candidate')
shutil.copytree(original,candidate)
source=original/'omni.ja'
target=candidate/'omni.ja'
patches={
 'chrome/noraneko/nora-resource/modules/pwa/SsbCommandLineHandler.sys.mjs':[
  ('extraOptions.setPropertyAsAString("ssbid", ssb.id);',
   'extraOptions.setPropertyAsAString("ssbid", ssb.id);\n'
   '\t\tif (AppConstants.platform === "linux") {\n'
   '\t\t\tconst { LinuxSupport } = ChromeUtils.importESModule("resource://noraneko/modules/pwa/supports/Linux.sys.mjs");\n'
   '\t\t\textraOptions.setPropertyAsAString("taskbartabclass", LinuxSupport.getWindowClass(ssb));\n'
   '\t\t}')],
 'chrome/noraneko/nora-resource/Linux.sys-DfscGtff.mjs':[
  ('\tstatic getPathInfo(ssb, dataHome',
   '\tstatic getWindowClass(ssb) {\n\t\treturn `floorp-${this.getNormalizedId(ssb)}`;\n\t}\n\tstatic getPathInfo(ssb, dataHome'),
  ('for (const candidate of candidates) if (candidate) {',
   'for (const candidate of candidates) if (typeof candidate === \"string\" && candidate) {'),
  ('const desktopBasename = `floorp-${this.getNormalizedId(ssb)}`;',
   'const desktopBasename = this.getWindowClass(ssb);')]
}
changed=[]
with zipfile.ZipFile(source) as reader, zipfile.ZipFile(target,'w') as writer:
    for entry in reader.infolist():
        data=reader.read(entry.filename)
        if entry.filename in patches:
            text=data.decode()
            for before,after in patches[entry.filename]:
                if text.count(before)!=1:
                    raise RuntimeError(f'Expected exactly one postimage selector: {entry.filename}: {before}')
                text=text.replace(before,after)
            patched=text.encode()
            changed.append({'path':entry.filename,'beforeSha256':hashlib.sha256(data).hexdigest(),
                'afterSha256':hashlib.sha256(patched).hexdigest()})
            data=patched
        writer.writestr(entry,data)
if len(changed)!=2:
    raise RuntimeError('Unexpected resource change count')
unchanged=0
for path in original.rglob('*'):
    if not path.is_file() or path==source:
        continue
    if hashlib.sha256(path.read_bytes()).digest()!=hashlib.sha256((candidate/path.relative_to(original)).read_bytes()).digest():
        raise RuntimeError('Unexpected product file change: '+str(path))
    unchanged+=1
(root/'pwa-class-candidate-provenance.json').write_text(json.dumps({
    'kind':'isolated patched product copy; not full source build or signed release',
    'sourceSha':'984509d2053be7bae9027133bda7259cd706b208',
    'changedResources':changed,'unchangedNonArchiveFiles':unchanged,
    'originalOmniSha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'candidateOmniSha256':hashlib.sha256(target.read_bytes()).hexdigest(),
},indent=2))
