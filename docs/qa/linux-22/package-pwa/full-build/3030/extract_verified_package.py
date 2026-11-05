#!/usr/bin/env python3
"""Verify a GitHub artifact digest and safely extract its unchanged Linux package."""
from pathlib import Path,PurePosixPath
import argparse,json,zipfile,tarfile,hashlib,configparser
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zip',type=Path,required=True);p.add_argument('--metadata',type=Path,required=True);p.add_argument('--destination',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
EXPECTED='e55152c559a141001a375ac77098d141b0db3ca7';RUN=42067997767;m=json.loads(a.metadata.read_text());assert m['workflow_run']['id']==RUN and m['workflow_run']['head_sha']==EXPECTED
assert m['name']=='noraneko-linux-x86_64-installer-unsigned' and not m['expired']
sha=hashlib.sha256(a.zip.read_bytes()).hexdigest();assert m['digest']=='sha256:'+sha,(m['digest'],sha)
existing=a.destination.exists();a.destination.mkdir(parents=True,exist_ok=True);a.evidence.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.zip) as z:
 names=[n for n in z.namelist() if not n.endswith('/')];assert names==['floorp-linux-x86_64.tar.xz'],names
 package=a.destination/'floorp-linux-x86_64.tar.xz';tarBytes=z.read(names[0])
 if existing:assert package.is_file() and hashlib.sha256(package.read_bytes()).digest()==hashlib.sha256(tarBytes).digest(),'existing package archive must match authenticated ZIP'
 else:package.write_bytes(tarBytes)
with tarfile.open(package,'r:xz') as t:
 members=t.getmembers();assert members and all(not PurePosixPath(x.name).is_absolute() and '..' not in PurePosixPath(x.name).parts for x in members)
 if not existing:t.extractall(a.destination,filter='data')
 for member in members:
  if member.isfile():
   target=a.destination/member.name;assert target.is_file() and hashlib.sha256(target.read_bytes()).digest()==hashlib.sha256(t.extractfile(member).read()).digest(),member.name
product=a.destination/'floorp';assert (product/'floorp').is_file()
changes=[]
for archivePath in [product/'omni.ja',product/'browser/omni.ja']:
 with zipfile.ZipFile(archivePath) as z:
  for name in z.namelist():
   if name.endswith('.manifest'):
    for line in z.read(name).decode().splitlines():
     if 'override chrome://branding/locale/brand.properties' in line:changes.append({'archive':archivePath.relative_to(product).as_posix(),'member':name,'registration':line})
assert len(changes)==1 and changes[0]['registration']=='override chrome://branding/locale/brand.properties resource:///chrome/en-US/locale/branding/brand.properties',changes
ini=configparser.ConfigParser(interpolation=None);ini.read(product/'application.ini')
record={'packageKind':'workflow-generated-linux-package','sourceSha':EXPECTED,'workflowRunId':RUN,'workflowArtifactId':m['id'],'artifactName':m['name'],'workflowArtifactDigest':m['digest'],'downloadedArtifactSha256':sha,'workflowArtifactArchive':str(a.zip.resolve()),'packageArchive':str(package.resolve()),'packageArchiveSha256':hashlib.sha256(package.read_bytes()).hexdigest(),'productRoot':str(product.resolve()),'generatedBrandingOverrides':changes,'applicationIni':{s:dict(ini[s]) for s in ini.sections()},'manualProductPatch':False,'nativeRecompilationClaim':False,'releaseOrSigningClaim':False}
(a.evidence/'package-provenance.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
