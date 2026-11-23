"""Reconstruct the immutable input and package only the explicit repair payload."""
import difflib
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import zipfile

BASE = Path('/workspace/qa-review-v030')
ORIGINAL = BASE / 'inputs/github-87311566/extracted/Floorp_QA_Implementation_Kit_v0.3.0'
FIXED = BASE / 'fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0'
WORKTREE = BASE / 'worktrees/Floorp-kit-fixes'
SOURCE_ZIP = BASE / 'inputs/github-87311566/Floorp_QA_Implementation_Kit_v0.3.0.zip'
DEST = WORKTREE / 'docs/qa/kit-v0.3.0-review-fixes'
PREFIX = ORIGINAL.name
EXPECTED_SOURCE_SHA = 'a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50'
ADDED = {'REVIEW_FIXES.md', 'validation/review-fixes.json',
         'tools/qa/tests/test_review_regressions.py'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    assert sha(SOURCE_ZIP.read_bytes()) == EXPECTED_SOURCE_SHA
    assert SOURCE_ZIP.stat().st_size == 150471
    original_names = {str(p.relative_to(ORIGINAL)) for p in ORIGINAL.rglob('*') if p.is_file()}
    assert len(original_names) == 57
    for line in (ORIGINAL / 'MANIFEST.sha256').read_text().splitlines():
        expected, name = line.split('  ', 1)
        assert sha((ORIGINAL / name).read_bytes()) == expected
    names = sorted(original_names | ADDED)
    actual = {str(p.relative_to(FIXED)) for p in FIXED.rglob('*') if p.is_file()}
    assert actual == set(names), f'unexpected repair files: {actual ^ set(names)}'
    for name in names:
        assert not (FIXED / name).is_symlink()
    document_names = ['DESIGN.md', *sorted(n for n in original_names if n.startswith('docs/'))]
    assert all((ORIGINAL / n).read_bytes() == (FIXED / n).read_bytes() for n in document_names)
    manifest = ''.join(f'{sha((FIXED / name).read_bytes())}  {name}\n'
                       for name in names if name != 'MANIFEST.sha256')
    (FIXED / 'MANIFEST.sha256').write_text(manifest)
    DEST.mkdir(parents=True, exist_ok=True)
    archive = DEST / 'Floorp_QA_Implementation_Kit_v0.3.0_review-fixes.1.zip'
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as output:
        for name in names:
            info = zipfile.ZipInfo(PREFIX + '/' + name, date_time=(1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            output.writestr(info, (FIXED / name).read_bytes(), compresslevel=9)
    patch_parts = []
    changed = []
    for name in names:
        before = (ORIGINAL / name).read_bytes() if name in original_names else b''
        after = (FIXED / name).read_bytes()
        if before == after:
            continue
        changed.append(name)
        patch_parts.extend(difflib.unified_diff(
            before.decode('utf-8').splitlines(keepends=True),
            after.decode('utf-8').splitlines(keepends=True),
            fromfile='a/' + name if name in original_names else '/dev/null',
            tofile='b/' + name))
    patch = DEST / 'kit-fixes.patch'
    patch.write_text(''.join(patch_parts))
    original_destination = WORKTREE / 'docs/Floorp_QA_Implementation_Kit_v0.3.0.zip'
    assert not original_destination.exists()
    shutil.copyfile(SOURCE_ZIP, original_destination)

    # Expand the final bytes into a new directory with explicit path/type budgets.
    expanded = BASE / 'fixes/archive-verification'
    expanded.mkdir(exist_ok=False)
    total = 0
    seen = set()
    with zipfile.ZipFile(archive) as source:
        members = source.infolist()
        assert len(members) == len(names) <= 100
        assert archive.stat().st_size <= 4 * 1024**2
        assert source.testzip() is None
        for info in members:
            item = PurePosixPath(info.filename)
            assert info.filename == item.as_posix() and not item.is_absolute()
            assert '..' not in item.parts and '\\' not in info.filename
            assert item.parts[0] == PREFIX and len(item.parts) >= 2
            assert not info.flag_bits & 1 and not info.is_dir()
            assert stat.S_IFMT(info.external_attr >> 16) == stat.S_IFREG
            assert info.filename not in seen
            seen.add(info.filename)
            total += info.file_size
            assert total <= 8 * 1024**2
            data = source.read(info)
            assert len(data) == info.file_size
            destination = expanded.joinpath(*item.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            with destination.open('xb') as stream:
                stream.write(data)
            assert data == (FIXED / Path(*item.parts[1:])).read_bytes()
    for line in (expanded / PREFIX / 'MANIFEST.sha256').read_text().splitlines():
        expected, name = line.split('  ', 1)
        assert sha((expanded / PREFIX / name).read_bytes()) == expected

    # Prove the plaintext patch reconstructs exactly the same repaired payload.
    roundtrip = BASE / 'fixes/patch-verification' / PREFIX
    shutil.copytree(ORIGINAL, roundtrip)
    subprocess.run(['git', 'apply', '--no-index', '--check', str(patch)],
                   cwd=roundtrip, check=True)
    subprocess.run(['git', 'apply', '--no-index', str(patch)], cwd=roundtrip, check=True)
    roundtrip_names = {str(p.relative_to(roundtrip)) for p in roundtrip.rglob('*') if p.is_file()}
    assert roundtrip_names == set(names)
    assert all((roundtrip / n).read_bytes() == (FIXED / n).read_bytes() for n in names)

    result = {'schemaVersion': 1, 'repairRevision': 'review-fixes.1',
              'originalSourceCommit': '87311566a225936fbe7948db142bce3f1d3379d8',
              'originalGitBlob': 'e688e2725ce502f36c600cfec88060e7954b30e2',
              'originalZipSha256': EXPECTED_SOURCE_SHA,
              'originalZipBytes': SOURCE_ZIP.stat().st_size,
              'repairedZip': archive.name, 'repairedZipBytes': archive.stat().st_size,
              'repairedZipSha256': sha(archive.read_bytes()),
              'patchSha256': sha(patch.read_bytes()), 'patchRoundtripExact': True,
              'originalDesignDocumentsUnchanged': True,
              'archiveEntries': len(names), 'payloadManifestEntries': len(names) - 1,
              'expandedBytes': total, 'crcAndSafeExpansion': 'PASS',
              'generatedCachesIncluded': False, 'changedPayloadPaths': changed,
              'publicationAuthorized': False, 'nativeQualification': 'UNVERIFIED'}
    (DEST / 'archive-validation.json').write_text(json.dumps(result, indent=2) + '\n')
    (BASE / 'evidence/qa307-archive-validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
