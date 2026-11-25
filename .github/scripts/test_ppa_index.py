#!/usr/bin/env python3
"""Regression tests for withholding, signed metadata hashes, and real APT policy."""

import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from ppa_index import candidate_version, filter_index, latest_candidate, paragraphs, product_version, verify_apt_candidates, verify_release, withheld_versions


DIGEST = "f" * 64


def record(version, package="floorp", filename_version=None):
    filename_version = version if filename_version is None else filename_version
    return (
        f"Package: {package}\nVersion: {version}\nArchitecture: amd64\n"
        f"Filename: ./floorp-{filename_version}.deb\nSize: 113309570\nSHA256: {DIGEST}\n"
        "Description: Test metadata\n continuation retained\n\n"
    ).encode()


class PpaIndexTests(unittest.TestCase):
    def test_live_candidate_selection_accepts_future_releases_in_debian_order(self):
        before = record("12.9.2~build1", filename_version="12.9.2") + record("12.19.0~build1", filename_version="12.19.0")
        self.assertEqual(latest_candidate(before)["Version"], "12.19.0~build1")
        future = record("12.21.0~build1", filename_version="12.21.0")
        after, _ = filter_index(before + record("12.20.0~build1", filename_version="12.20.0") + future, {"12.20.0"})
        candidate = latest_candidate(after)
        self.assertEqual(candidate["Version"], "12.21.0~build1")
        self.assertEqual(filter_index(after, {"12.20.0"}, product_version(candidate["Version"]), candidate["SHA256"]), (after, []))

    def test_authenticated_build1_shape_preserves_records_and_withholds_12_20(self):
        older = record("11.26.0") + record("12.18.1~build1", filename_version="12.18.1")
        candidate = record("12.19.0~build1", filename_version="12.19.0")
        held = record("12.20.0~build1", filename_version="12.20.0")
        other = record("12.20.0~build1", "other-package", "12.20.0")
        after, removed = filter_index(older + candidate + held + other, {"12.20.0"}, "12.19.0", DIGEST)
        self.assertEqual(after, older + candidate + other)
        self.assertEqual(removed, ["floorp-12.20.0.deb"])
        self.assertEqual(candidate_version(after, "12.19.0"), "12.19.0~build1")
        self.assertEqual(filter_index(after, {"12.20.0"}, "12.19.0", DIGEST), (after, []))

    def test_build1_cannot_disguise_another_release_or_unsupported_suffix(self):
        for before in [record("12.20.0~build1", filename_version="12.19.0"),
                       record("12.20.0~build2", filename_version="12.20.0"),
                       record("12.20.0~build1", filename_version="12.20.0~build1")]:
            with self.assertRaises(ValueError):
                filter_index(before, {"12.20.0"})

    def test_build1_keeps_asset_digest_and_candidate_uniqueness_guards(self):
        candidate = record("12.19.0~build1", filename_version="12.19.0")
        with self.assertRaisesRegex(ValueError, "verified Release"):
            filter_index(candidate, set(), "12.19.0", "0" * 64)
        with self.assertRaisesRegex(ValueError, "exactly once"):
            filter_index(candidate + record("12.19.0"), set(), "12.19.0", DIGEST)

    def test_build1_candidate_uses_real_debian_ordering(self):
        candidate = record("12.19.0~build1", filename_version="12.19.0")
        with self.assertRaisesRegex(ValueError, "newer"):
            filter_index(candidate + record("12.20.0~build1", filename_version="12.20.0"), set(), "12.19.0", DIGEST)

    def test_new_publish_guard_rejects_real_withheld_deb_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            policy = Path(temporary).joinpath("policy.json")
            policy.write_text(json.dumps({"schemaVersion": 1, "versions": ["12.20.0"]}))
            script = Path(__file__).with_name("ppa_index.py")
            for version in ["12.20.0", "12.20.0~build1"]:
                rejected = subprocess.run([sys.executable, str(script), "check-new", str(policy), version], capture_output=True, text=True)
                self.assertNotEqual(rejected.returncode, 0)
                self.assertIn("Refusing to publish withheld Floorp", rejected.stderr)
            subprocess.run([sys.executable, str(script), "check-new", str(policy), "12.19.0~build1"], check=True)
        self.assertEqual(product_version("12.19.0~build1"), "12.19.0")

    def test_only_withheld_floorp_record_is_removed(self):
        before = record("12.18.1") + record("12.19.0") + record("12.20.0") + record("12.20.0", "other-package")
        after, removed = filter_index(before, {"12.20.0"}, "12.19.0", DIGEST)
        self.assertEqual(after, record("12.18.1") + record("12.19.0") + record("12.20.0", "other-package"))
        self.assertEqual(removed, ["floorp-12.20.0.deb"])

    def test_repeated_withholding_is_idempotent(self):
        after, _ = filter_index(record("12.19.0") + record("12.20.0"), {"12.20.0"})
        self.assertEqual(filter_index(after, {"12.20.0"}), (after, []))

    def test_future_release_preserves_hold(self):
        after, _ = filter_index(record("12.19.0") + record("12.20.0") + record("12.21.0"), {"12.20.0"})
        self.assertEqual(after, record("12.19.0") + record("12.21.0"))

    def test_newer_candidate_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "newer"):
            filter_index(record("12.19.0") + record("12.21.0"), {"12.20.0"}, "12.19.0", DIGEST)

    def test_missing_or_duplicate_candidate_is_rejected(self):
        for before in [record("12.18.1"), record("12.19.0") * 2]:
            with self.assertRaisesRegex(ValueError, "exactly once"):
                filter_index(before, set(), "12.19.0", DIGEST)

    def test_release_asset_digest_must_match(self):
        with self.assertRaisesRegex(ValueError, "verified Release"):
            filter_index(record("12.19.0"), set(), "12.19.0", "0" * 64)

    def test_all_candidates_cannot_be_removed(self):
        with self.assertRaisesRegex(ValueError, "all Floorp"):
            filter_index(record("12.20.0"), {"12.20.0"})

    def test_record_validation(self):
        for before in [record("12.19.0").replace(b"floorp-12.19.0.deb", b"../escape.deb"),
                       record("12.19.0").replace(b"Architecture: amd64", b"Architecture: arm64"),
                       record("12.19.0").replace(DIGEST.encode(), b"invalid"),
                       record("12.19.0").replace(b"Version: 12.19.0", b"Version: 12.19.0\nVersion: duplicate")]:
            with self.assertRaises(ValueError):
                filter_index(before, set())

    def test_policy_validation(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary).joinpath("policy.json")
            for policy in [{"schemaVersion": 1, "versions": ["../escape"]},
                           {"schemaVersion": 1, "versions": ["12.20.0", "12.20.0"]},
                           {"schemaVersion": 2, "versions": []}]:
                path.write_text(json.dumps(policy))
                with self.assertRaises(ValueError):
                    withheld_versions(path)
            path.write_text(json.dumps({"schemaVersion": 1, "versions": ["12.20.0"]}))
            self.assertEqual(withheld_versions(path), {"12.20.0"})

    def test_signed_release_checksums_and_compression(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            data = record("12.19.0")
            root.joinpath("Packages").write_bytes(data)
            root.joinpath("Packages.gz").write_bytes(gzip.compress(data, mtime=0))
            release = "SHA256:\n"
            for name in ["Packages", "Packages.gz"]:
                content = root.joinpath(name).read_bytes()
                release += f" {hashlib.sha256(content).hexdigest()} {len(content)} {name}\n"
            root.joinpath("Release").write_text(release)
            verify_release(root)
            root.joinpath("Packages").write_bytes(data + b"\n")
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                verify_release(root)

    def test_actual_apt_new_install_and_existing_12_20(self):
        after, _ = filter_index(record("12.18.1") + record("12.19.0") + record("12.20.0"), {"12.20.0"})
        verify_apt_candidates(after, "12.19.0", "12.20.0")
        self.assertEqual(len(paragraphs(after)), 2)

    def test_actual_apt_build1_new_install_and_existing_12_20(self):
        after, _ = filter_index(record("12.18.1~build1", filename_version="12.18.1")
                               + record("12.19.0~build1", filename_version="12.19.0")
                               + record("12.20.0~build1", filename_version="12.20.0"), {"12.20.0"})
        verify_apt_candidates(after, "12.19.0", "12.20.0~build1")
        verify_apt_candidates(after, "12.19.0", "12.20.0")


if __name__ == "__main__":
    unittest.main()
