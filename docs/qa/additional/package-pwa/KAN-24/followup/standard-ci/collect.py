"""Collect standard CI metadata, digest-verified artifacts, and target execution excerpts."""
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent
REPO = 'Floorp-Projects/Floorp'
RUNS = [42093984715, 42094068516]

def gh_json(*args):
    return json.loads(subprocess.check_output(['gh', *args]))

for run in RUNS:
    meta = gh_json('run', 'view', str(run), '--repo', REPO, '--json', 'status,conclusion,headSha,jobs,url')
    (ROOT / f'{run}-metadata.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(run, meta['status'], meta['conclusion'])
    artifacts = gh_json('api', f'repos/{REPO}/actions/runs/{run}/artifacts')
    (ROOT / f'{run}-artifacts.json').write_text(json.dumps(artifacts, indent=2) + '\n')
    for artifact in artifacts['artifacts']:
        if artifact['expired']:
            continue
        archive = ROOT / f"{run}-{artifact['id']}.zip"
        if not archive.exists() or archive.stat().st_size == 0:
            with archive.open('wb') as out:
                download = subprocess.run(['gh', 'api', f"repos/{REPO}/actions/artifacts/{artifact['id']}/zip"], stdout=out, stderr=subprocess.PIPE)
            if download.returncode:
                archive.unlink(missing_ok=True)
                print('artifact download unavailable via gh; use connected GitHub tool', artifact['id'])
                continue
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        expected = artifact.get('digest')
        if expected and expected != 'sha256:' + digest:
            raise RuntimeError(f'Digest mismatch for {archive.name}')
        evidence = ROOT / f'{run}-evidence'
        evidence.mkdir(exist_ok=True)
        excerpts = []
        with zipfile.ZipFile(archive) as zipped:
            for name in zipped.namelist():
                if not name.endswith('.log'):
                    continue
                content = zipped.read(name).decode('utf-8', errors='replace')
                # Only public build/test logs, no runner home or browser profiles.
                if 'latest.log' in name or 'ci-combined-browser-test.log' in name or 'ci-feles-build-test.log' in name:
                    destination = evidence / name.replace('/', '__')
                    destination.write_text(content)
                lines = content.splitlines()
                for i, line in enumerate(lines):
                    if any(s in line for s in ['vertical-tabs-observer.test', 'verticalTabsSync.test', 'Summary:', 'Downloaded Firefox coverage:', '[META] exitCode=', 'appBuildID', 'appVersion', 'Services.version']):
                        excerpts.append({'file': name, 'line': i + 1, 'text': '\n'.join(lines[max(0, i - 1):i + 3])})
        result = {'run': run, 'headSha': meta['headSha'], 'artifactId': artifact['id'], 'artifactName': artifact['name'], 'apiDigest': expected, 'actualSha256': digest, 'digestVerified': expected == 'sha256:' + digest, 'excerpts': excerpts}
        (ROOT / f'{run}-artifact-verification.json').write_text(json.dumps(result, indent=2) + '\n')
        print('artifact', artifact['id'], 'digestVerified', result['digestVerified'], 'excerpts', len(excerpts))
