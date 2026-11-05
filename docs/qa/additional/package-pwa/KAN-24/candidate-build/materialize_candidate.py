#!/usr/bin/env python3
"""Scoped production frontend substitution; not a full product build."""
from pathlib import Path
import copy,hashlib,json,re,shutil,zipfile
QA=Path('/workspace/qa-linux-additional/KAN-24')
BASE=Path('/workspace/qa-linux-full-build/products/3040-r2/floorp')
SOURCE=Path('/workspace/qa-kan24-candidate')
OUT=QA/'candidate-product/floorp'
sha=lambda b:hashlib.sha256(b).hexdigest()
assert not OUT.exists(), 'Refuse overwrite'
components=[('bridge/loader-features','nora-content/'),('browser-features/pages-settings','nora-content-settings/')]
generated={}; manifests={}
for component,prefix in components:
 d=SOURCE/component/'_dist'; manifest=(d/'jar.mn').read_text(); manifests[component]= {'sha256':sha(manifest.encode()),'text':manifest}
 for line in manifest.splitlines():
  m=re.match(r'\s+(\S+)\s+\((.+)\)$',line)
  if m:
   assert m[1].startswith(prefix)
   generated['chrome/noraneko/'+m[1]]=(d/m[2]).read_bytes()
prefixes=tuple('chrome/noraneko/'+p for _,p in components)
with zipfile.ZipFile(BASE/'omni.ja') as old:
 old_names=set(old.namelist()); old_scope={n for n in old_names if n.startswith(prefixes)}
 removed=old_scope-set(generated)
 changed={n:v for n,v in generated.items() if n not in old_names or old.read(n)!=v}
 # Existing registrations cover both unchanged package paths; jar.mn is a build input.
 regs=old.read('chrome/chrome.manifest').decode()
 assert 'noraneko-settings' in regs and 'nora-content-settings/' in regs
 assert 'nora-content/' in regs
 shutil.copytree(BASE,OUT,symlinks=True)
 ledger=[]
 with zipfile.ZipFile(OUT/'omni.ja','w') as new:
  new.comment=old.comment
  for info in old.infolist():
   n=info.filename
   if n in removed:
    ledger.append({'path':n,'operation':'remove','before':sha(old.read(n))});continue
   new.writestr(copy.copy(info),changed.get(n,old.read(n)))
  for n,data in changed.items():
   if n not in old_names:new.writestr(n,data,compress_type=zipfile.ZIP_DEFLATED)
   ledger.append({'path':n,'operation':'replace' if n in old_names else 'add','before':sha(old.read(n)) if n in old_names else None,'after':sha(data)})
with zipfile.ZipFile(BASE/'omni.ja') as old,zipfile.ZipFile(OUT/'omni.ja') as new:
 assert new.testzip() is None
 altered={n for n in set(old.namelist())|set(new.namelist()) if n not in old.namelist() or n not in new.namelist() or old.read(n)!=new.read(n)}
 assert altered==set(changed)|removed
 assert all(n.startswith(prefixes) for n in altered)
 assert all(new.read(n)==v for n,v in generated.items())
 assert old.read('chrome/chrome.manifest')==new.read('chrome/chrome.manifest')
files=[]
for p in sorted(BASE.rglob('*')):
 rel=p.relative_to(BASE); q=OUT/rel
 if p.is_symlink(): assert q.is_symlink() and p.readlink()==q.readlink()
 elif p.is_file():files.append({'path':str(rel),'before':sha(p.read_bytes()),'after':sha(q.read_bytes())})
assert [f['path'] for f in files if f['before']!=f['after']]==['omni.ja']
assert {str(p.relative_to(BASE)) for p in BASE.rglob('*')}=={str(p.relative_to(OUT)) for p in OUT.rglob('*')}
intake=Path('/workspace/qa-linux-22/package-pwa/wayland-browser-identity/full-package/intake-r2/product-intake.json')
result={'scope':'authenticated3040-r2 product copy with ONLY generated chrome/settings components; NOT full product/source build','baseProduct':str(BASE),'candidateProduct':str(OUT),'baseIntake':str(intake),'baseIntakeSha256':sha(intake.read_bytes()),'sourceSubject':json.loads((QA/'candidate-build/source-subject.json').read_text()),'manifestInputs':manifests,'registrationManifestUnchanged':True,'resourceChanges':ledger,'productInventory':files,'onlyProductFileChanged':'omni.ja','allUnrelatedResourcesAndNativeFilesUnchanged':True}
(QA/'candidate-build/candidate-resource-provenance.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'candidateProduct':str(OUT),'changedResources':len(changed),'removedResources':len(removed),'allUnrelatedResourcesAndNativeFilesUnchanged':True}))
