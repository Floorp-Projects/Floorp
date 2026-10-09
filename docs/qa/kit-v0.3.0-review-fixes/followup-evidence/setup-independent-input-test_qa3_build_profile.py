# SPDX-License-Identifier: MPL-2.0

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from qa3_build_profile import (
    TARGETS,
    ac_options,
    enable_debug_tests,
    profile,
    verify_config,
)

ROOT = Path(__file__).resolve().parents[3]
CONFIGS = {
    ("linux", "x86_64"): "linux-x86_64",
    ("linux", "aarch64"): "linux-aarch64",
    ("windows", "x86_64"): "windows-x86_64",
    ("mac", "x86_64"): "macosx64-x86_64",
    ("mac", "aarch64"): "macosx64-aarch64",
}


class BuildProfileTests(unittest.TestCase):
    def setup_result(
        self,
        platform,
        arch,
        debug,
        pgo,
        mode="",
        artifact="",
        config_suffix="",
        arguments=None,
    ):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scripts = root / ".github/workflows/scripts"
            scripts.mkdir(parents=True)
            configs = root / ".github/workflows/mozconfigs"
            configs.mkdir()
            for file in (ROOT / ".github/workflows/mozconfigs").glob("*.mozconfig"):
                (configs / file.name).write_text(file.read_text() + config_suffix)
            for name in ("setup-floorp.sh", "qa3_build_profile.py"):
                shutil.copy2(ROOT / ".github/workflows/scripts" / name, scripts / name)
            patches = root / ".github/patches/debug"
            patches.mkdir(parents=True)
            (patches / "fixture.patch").write_text("fixture patch\n")
            (root / "build").mkdir()
            update_file = root / "build/application.ini.in"
            update_file.write_text("fixture\n")
            branding = root / ".github/assets/branding"
            branding.mkdir(parents=True)
            (branding / "fixture-branding").write_text("fixture branding\n")
            (root / "browser/branding").mkdir(parents=True)
            effects = root / "effects"
            bin_dir = root / "bin"
            bin_dir.mkdir()
            for path in (
                bin_dir / "git",
                bin_dir / "sudo",
                bin_dir / "rustup",
                root / "mach",
            ):
                path.write_text(
                    '#!/bin/sh\nprintf "%s\\n" "$0 $*" >> "$QA3_EFFECTS"\nexit 0\n'
                )
                path.chmod(0o700)
            env = {
                "PATH": f"{bin_dir}:/usr/bin:/bin",
                "GITHUB_WORKSPACE": str(root),
                "SCCACHE_PATH": "/fixture/sccache",
                "QA3_EFFECTS": str(effects),
                "PYTHONDONTWRITEBYTECODE": "1",
            }
            if arguments is None:
                arguments = [
                    platform,
                    arch,
                    str(debug).lower(),
                    str(pgo).lower(),
                    mode,
                    artifact,
                    "20261009000000",
                ]
            result = subprocess.run(
                ["bash", str(scripts / "setup-floorp.sh"), *arguments],
                cwd=root,
                env=env,
                check=False,
                capture_output=True,
                text=True,
                timeout=15,
            )
            config = root / "mozconfig"
            return {
                "process": result,
                "config": config.read_text() if config.exists() else None,
                "effects": effects.read_text() if effects.exists() else "",
                "updateUnchanged": update_file.read_text() == "fixture\n",
                "brandingCopied": (root / "browser/branding/fixture-branding").exists(),
            }

    def generated(self, platform, arch, debug, pgo, mode="", artifact=""):
        result = self.setup_result(platform, arch, debug, pgo, mode, artifact)
        process = result["process"]
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertIn("bootstrap --application-choice browser", result["effects"])
        return result["config"]

    def test_real_setup_rejects_invalid_inputs_before_side_effects(self):
        for args in (
            ("windows", "aarch64", False, False, "", ""),
            ("linux", "aarch64", False, True, "generate", ""),
            ("linux", "x86_64", True, False, "use", "old-profile"),
            ("mac", "aarch64", False, True, "use", ""),
            ("mac", "x86_64", False, True, "generate", "old-profile"),
            ("linux", "x86_64", False, True, "invalid-mode", ""),
        ):
            with self.subTest(args=args):
                result = self.setup_result(*args)
                process = result["process"]
                self.assertNotEqual(
                    process.returncode,
                    0,
                    f"stdout={process.stdout!r}\nstderr={process.stderr!r}\n"
                    f"effects={result['effects']!r}",
                )
                self.assertIsNone(result["config"])
                self.assertEqual(result["effects"], "")
                self.assertTrue(result["updateUnchanged"])
                self.assertFalse(result["brandingCopied"])

    def test_real_setup_rejects_wrong_generated_config_before_bootstrap(self):
        for debug, suffix in (
            (False, "ac_add_options --target=aarch64-unknown-linux-gnu\n"),
            (False, "ac_add_options --enable-debug\n"),
            (False, "ac_add_options --enable-profile-generate=cross\n"),
            (True, "ac_add_options --disable-tests --other\n"),
        ):
            with self.subTest(debug=debug, suffix=suffix):
                result = self.setup_result(
                    "linux", "x86_64", debug, False, config_suffix=suffix
                )
                process = result["process"]
                self.assertNotEqual(
                    process.returncode,
                    0,
                    f"stdout={process.stdout!r}\nstderr={process.stderr!r}\n"
                    f"effects={result['effects']!r}",
                )
                self.assertNotIn("sudo", result["effects"])
                self.assertNotIn("rustup", result["effects"])
                self.assertNotIn("bootstrap", result["effects"])

    def test_real_setup_rejects_invalid_booleans_and_incomplete_modes(self):
        for args in (
            ("unknown", "x86_64", False, False),
            ("linux", "unknown", False, False),
            ("linux", "x86_64", "invalid", False),
            ("linux", "x86_64", False, "invalid"),
            ("linux", "x86_64", False, False, "generate"),
            ("linux", "x86_64", False, False, "", "old-profile"),
            ("linux", "x86_64", False, True),
        ):
            with self.subTest(args=args):
                result = self.setup_result(*args)
                process = result["process"]
                self.assertNotEqual(process.returncode, 0, process.stderr)
                self.assertIsNone(result["config"])
                self.assertEqual(result["effects"], "")
                self.assertTrue(result["updateUnchanged"])
                self.assertFalse(result["brandingCopied"])

    def test_real_setup_argument_count_and_optional_defaults(self):
        for arguments in (
            ["linux", "x86_64", "false"],
            ["linux", "x86_64", "false", "false", "", "", "", "extra"],
        ):
            with self.subTest(arguments=arguments):
                result = self.setup_result(
                    "linux", "x86_64", False, False, arguments=arguments
                )
                self.assertNotEqual(result["process"].returncode, 0)
                self.assertIsNone(result["config"])
                self.assertEqual(result["effects"], "")
        result = self.setup_result(
            "linux",
            "x86_64",
            False,
            False,
            arguments=["linux", "x86_64", "false", "false"],
        )
        self.assertEqual(result["process"].returncode, 0, result["process"].stderr)
        verify_config(result["config"], profile("linux", "x86_64", False, False))

    def test_real_setup_generates_canonical_debug_on_all_five_targets(self):
        for platform, arch in TARGETS:
            with self.subTest(platform=platform, arch=arch):
                result = self.generated(platform, arch, True, False)
                self.assertNotIn("--disable-tests", ac_options(result))
                self.assertEqual(ac_options(result).count("--enable-tests"), 1)
                evidence = verify_config(result, profile(platform, arch, True, False))
                self.assertFalse(evidence["configureExecuted"])
                self.assertFalse(evidence["nativeTestsExecuted"])

    def test_production_opt_and_pgo_preserve_tests_and_profile_flags(self):
        for platform, arch in TARGETS:
            base = (
                ROOT
                / f".github/workflows/mozconfigs/{CONFIGS[(platform, arch)]}.mozconfig"
            ).read_text()
            wanted_tests = [v for v in ac_options(base) if v.endswith("-tests")]
            modes = (
                [""]
                if (platform, arch) == ("linux", "aarch64")
                else ["", "generate", "use"]
            )
            for mode in modes:
                with self.subTest(platform=platform, arch=arch, mode=mode):
                    artifact = "fixture-profile" if mode == "use" else ""
                    result = self.generated(
                        platform, arch, False, bool(mode), mode, artifact
                    )
                    self.assertEqual(
                        [v for v in ac_options(result) if v.endswith("-tests")],
                        wanted_tests,
                    )
                    verify_config(
                        result,
                        profile(platform, arch, False, bool(mode), mode, artifact),
                    )

    def test_legacy_debug_pgo_is_not_canonical_qualification(self):
        for platform, arch in TARGETS:
            if (platform, arch) == ("linux", "aarch64"):
                continue
            for mode in ("generate", "use"):
                with self.subTest(platform=platform, arch=arch, mode=mode):
                    artifact = "fixture-profile" if mode == "use" else ""
                    expected = profile(
                        platform,
                        arch,
                        True,
                        True,
                        mode,
                        artifact,
                        allow_legacy_debug_pgo=True,
                    )
                    self.assertFalse(expected["testsOverride"])
                    self.assertFalse(expected["qualificationEligible"])
                    self.assertEqual(expected["role"], f"legacy-debug-profile-{mode}")
                    result = self.generated(platform, arch, True, True, mode, artifact)
                    self.assertNotIn("--enable-tests", ac_options(result))
                    verify_config(result, expected)

    def test_legacy_debug_pgo_requires_explicit_noncanonical_policy(self):
        with self.assertRaises(ValueError):
            profile("linux", "x86_64", True, True, "generate")
        result = self.setup_result("linux", "x86_64", True, True, "generate")
        self.assertEqual(result["process"].returncode, 0, result["process"].stderr)
        self.assertIn("noncanonical", result["process"].stderr)
        self.assertIn("ineligible for qualification", result["process"].stderr)

    def test_invalid_targets_modes_and_profile_inputs_are_rejected(self):
        for args in (
            ("windows", "aarch64", False, False),
            ("linux", "aarch64", False, True, "generate"),
            ("linux", "x86_64", True, False, "use", "old-profile"),
            ("mac", "aarch64", False, True, "use"),
            ("mac", "x86_64", False, True, "generate", "old-profile"),
            ("linux", "x86_64", "false", False),
        ):
            with self.subTest(args=args), self.assertRaises(ValueError):
                profile(*args)

    def test_wrong_target_debug_tests_and_profiling_are_rejected(self):
        text = self.generated("mac", "aarch64", True, False)
        expected = profile("mac", "aarch64", True, False)
        changes = [
            text.replace("aarch64-apple-darwin", "x86_64-apple-darwin"),
            text.replace("--enable-tests", "--disable-tests"),
            text.replace("--enable-debug", "--disable-debug"),
            text + "ac_add_options --enable-profile-generate=cross\n",
            text + "ac_add_options --enable-tests\n",
        ]
        for changed in changes:
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                verify_config(changed, expected)

    def test_profile_use_requires_lto_profdata_and_jarlog(self):
        text = self.generated("linux", "x86_64", False, True, "use", "fixture-profile")
        expected = profile("linux", "x86_64", False, True, "use", "fixture-profile")
        for marker in (
            "export MOZ_LTO=cross",
            "ac_add_options --with-pgo-profile-path",
            "ac_add_options --with-pgo-jarlog",
        ):
            changed = "\n".join(
                line for line in text.splitlines() if not line.startswith(marker)
            )
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                verify_config(changed, expected)

    def test_debug_test_override_is_idempotent_and_rejects_ambiguous_flags(self):
        text = "# ac_add_options --disable-tests\nac_add_options --disable-tests\n"
        result = enable_debug_tests(text, True, False)
        self.assertEqual(enable_debug_tests(result, True, False), result)
        with self.assertRaises(ValueError):
            enable_debug_tests("ac_add_options --disable-tests --other\n", True, False)
        for debug, pgo in ((False, False), (False, True), (True, True)):
            self.assertEqual(enable_debug_tests(text, debug, pgo), text)


if __name__ == "__main__":
    unittest.main()
