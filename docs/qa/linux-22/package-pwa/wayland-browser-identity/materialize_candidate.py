#!/usr/bin/env python3
"""Create a disposable 3032 product copy with standard generated ESM output.

This is a controlled resource substitution, NOT a full product/source build.
No native, manifest, policy, preference or signature-check setting is changed.
"""
from pathlib import Path
import copy
import hashlib
import json
import shutil
import subprocess
import zipfile

QA = Path(__file__).resolve().parent
SOURCE = Path('/workspace/qa-linux-wayland-browser-fix')
BASE = Path('/workspace/qa-linux-full-build/products/3032/floorp')
OUT = QA / 'candidate-product/floorp'
DIST = SOURCE / 'bridge/loader-modules/_dist'
PREFIX = 'chrome/noraneko/nora-resource/'
sha = lambda data: hashlib.sha256(data).hexdigest()
if OUT.exists():
    raise RuntimeError('Candidate already exists; refuse to overwrite it')
shutil.copytree(BASE, OUT, symlinks=True)
generated = {str(p.relative_to(DIST)):p.read_bytes() for p in DIST.rglob('*.mjs')}
base_jar = BASE / 'omni.ja'
with zipfile.ZipFile(base_jar) as old:
    old_modules = {n[len(PREFIX):]:old.read(n) for n in old.namelist()
                   if n.startswith(PREFIX) and n.endswith('.mjs')}
    changed = {PREFIX+n:data for n,data in generated.items()
               if old_modules.get(n) != data}
    removed = {PREFIX+n for n in old_modules if n not in generated}
    allowed = {
        PREFIX+'modules/pwa/SsbCommandLineHandler.sys.mjs',
        PREFIX+'modules/pwa/supports/Linux.sys.mjs',
        PREFIX+'modules/pwa/test/SsbCommandLineHandler.test.mjs',
        PREFIX+'modules/pwa/supports/test/Linux.test.mjs',
    }
    assert all(n in allowed or (n.startswith(PREFIX+'Linux.sys-') and n.endswith('.mjs')) for n in changed)
    assert all(n.startswith(PREFIX+'Linux.sys-') and n.endswith('.mjs') for n in removed)
    target = OUT / 'omni.ja'
    ledger = []
    with zipfile.ZipFile(target, 'w') as new:
        new.comment = old.comment
        names = set(old.namelist())
        for info in old.infolist():
            if info.filename in removed:
                ledger.append({'path':info.filename,'operation':'remove-obsolete-generated-chunk','beforeSha256':sha(old.read(info.filename))})
                continue
            data = changed.get(info.filename, old.read(info.filename))
            new.writestr(copy.copy(info), data)
            if info.filename in changed:
                ledger.append({'path':info.filename,'operation':'replace-generated-resource',
                    'beforeSha256':sha(old.read(info.filename)),'afterSha256':sha(data)})
        for n,data in changed.items():
            if n not in names:
                new.writestr(n,data,compress_type=zipfile.ZIP_DEFLATED)
                ledger.append({'path':n,'operation':'add-generated-chunk','afterSha256':sha(data)})
with zipfile.ZipFile(base_jar) as old, zipfile.ZipFile(OUT/'omni.ja') as new:
    altered = {n for n in set(old.namelist())|set(new.namelist())
               if n not in old.namelist() or n not in new.namelist() or old.read(n)!=new.read(n)}
    assert altered == {r['path'] for r in ledger}
    assert all(n.startswith(PREFIX) and n.endswith('.mjs') for n in altered)
files = []
for p in sorted(BASE.rglob('*')):
    rel = p.relative_to(BASE)
    other = OUT / rel
    if p.is_symlink():
        assert other.is_symlink() and p.readlink()==other.readlink()
    elif p.is_file():
        before = sha(p.read_bytes())
        after = sha(other.read_bytes())
        files.append({'path':str(rel),'beforeSha256':before,'afterSha256':after,'unchanged':before==after})
assert [r['path'] for r in files if not r['unchanged']] == ['omni.ja']
source_paths = ['browser-features/modules/modules/pwa/SsbCommandLineHandler.sys.mts',
                'browser-features/modules/modules/pwa/supports/Linux.sys.mts',
                'browser-features/modules/modules/pwa/test/SsbCommandLineHandler.test.mts']
result = {'scope':'isolated exact3032 CI product copy with normal production-generated ESM substitutions; NOT full source/package build',
    'baseProduct':str(BASE),'candidateProduct':str(OUT),
    'baseCiRun':42068055038,'baseSourceSha':'8249e234adfbb3e877e4a20cf5f57d7264d9a447',
    'candidateSourceHead':subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip(),
    'candidateSourceState':'working tree changes by root; exact source SHA256 and diff digest below',
    'sourceSha256':{p:sha((SOURCE/p).read_bytes()) for p in source_paths},
    'sourceDiffSha256':sha(subprocess.check_output(['git','-C',str(SOURCE),'diff','--',*source_paths])),
    'denoLockUnchanged':subprocess.run(['git','-C',str(SOURCE),'diff','--exit-code','--','deno.lock'],capture_output=True).returncode==0,
    'buildId2':'01a1ac9e-ba04-71bb-a115-9231694b2853',
    'generatedModuleCount':len(generated),'resourceChanges':ledger,
    'productFileChanges':[r for r in files if not r['unchanged']],
    'nativeAndOtherProductFilesUnchanged':all(r['unchanged'] for r in files if r['path']!='omni.ja'),
    'productInventory':files}
(QA/'candidate-resource-provenance.json').write_text(json.dumps(result,indent=2))
print(json.dumps({k:result[k] for k in ['scope','candidateProduct','generatedModuleCount','resourceChanges','nativeAndOtherProductFilesUnchanged']},indent=2))
