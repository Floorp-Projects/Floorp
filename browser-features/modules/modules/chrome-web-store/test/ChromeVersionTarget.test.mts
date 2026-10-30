// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  CRX_DOWNLOAD_URLS,
  resolveChromeDownloadTarget,
  STORE_DOWNLOAD_CLIENT_VERSION,
} from "../Constants.sys.mts";
import { getChromeDownloadTarget } from "../ChromeExtensionUpdater.sys.mts";
import {
  checkChromeVersionRequirement,
  checkCompatibility,
  transformManifest,
  validateManifest,
} from "../ManifestTransformer.sys.mts";
import type { ChromeManifest } from "../types.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const id = "abcdefghijklmnopabcdefghijklmnop";
const manifest = (minimum?: unknown): ChromeManifest => ({
  manifest_version: 3,
  name: "Version requirement fixture",
  version: "1.0",
  ...(minimum !== undefined ? { minimum_chrome_version: minimum } : {}),
});

const tests: TestCase[] = [
  {
    name: "store reference is an explicit Chrome package-selection version",
    fn() {
      assertEquals(
        STORE_DOWNLOAD_CLIENT_VERSION,
        "155.0.8059.84",
        "download/reference version is independent of Gecko's version",
      );
    },
  },
  {
    name: "all platform variants retain the actual target in both URL attempts",
    fn() {
      for (
        const [platform, abi, os, arch] of [
          ["linux", "x86_64-gcc3", "linux", "x64"],
          ["linux", "aarch64-gcc3", "linux", "arm64"],
          ["macosx", "x86_64-gcc3", "mac", "x64"],
          ["macosx", "aarch64-gcc3", "mac", "arm64"],
          ["win", "x86_64-msvc", "win", "x64"],
          ["win", "aarch64-msvc", "win", "arm64"],
          ["win", "x86-msvc", "win", "x86"],
        ]
      ) {
        const target = resolveChromeDownloadTarget(platform, abi);
        assertEquals(target.os, os, `${platform}/${abi}: OS mapping`);
        assertEquals(
          target.arch,
          arch,
          `${platform}/${abi}: process architecture`,
        );
        for (
          const build of [CRX_DOWNLOAD_URLS.primary, CRX_DOWNLOAD_URLS.fallback]
        ) {
          const url = new URL(build(id, target));
          assertEquals(
            url.origin,
            "https://clients2.google.com",
            "official store origin",
          );
          assertEquals(
            url.pathname,
            "/service/update2/crx",
            "official CRX endpoint",
          );
          assertEquals(
            url.searchParams.get("os"),
            os,
            "retry does not change OS",
          );
          assertEquals(
            url.searchParams.get("arch"),
            arch,
            "retry does not change process architecture",
          );
          assertEquals(
            url.searchParams.get("prodversion"),
            STORE_DOWNLOAD_CLIENT_VERSION,
            "attempts select the same Chrome reference",
          );
          assertEquals(
            url.searchParams.get("x"),
            `id=${id}&uc`,
            "extension selection is encoded without changing the ID",
          );
          assertEquals(
            url.searchParams.get("acceptformat"),
            "crx2,crx3",
            "supported CRX formats retained",
          );
          assertEquals(
            url.searchParams.has("os_arch"),
            false,
            "native machine architecture is not guessed under emulation",
          );
          assertEquals(
            url.searchParams.has("nacl_arch"),
            false,
            "obsolete NaCl architecture is not fabricated",
          );
        }
      }
    },
  },
  {
    name:
      "unsupported OS and architecture fail before a substitute package is selected",
    fn() {
      for (
        const [platform, abi] of [
          ["android", "aarch64-gcc3"],
          ["freebsd", "x86_64-gcc3"],
          ["linux", "riscv64-gcc3"],
          ["win", "unknown"],
          ["", ""],
        ]
      ) {
        assertThrows(
          () => resolveChromeDownloadTarget(platform, abi),
          "unsupported target",
        );
      }
    },
  },
  {
    name: "native downloader target uses runtime OS and browser process ABI",
    fn() {
      const { AppConstants } = ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      );
      const actual = getChromeDownloadTarget();
      const expected = resolveChromeDownloadTarget(
        AppConstants.platform,
        Services.appinfo.XPCOMABI,
      );
      assertEquals(
        JSON.stringify(actual),
        JSON.stringify(expected),
        "native target matches runtime data",
      );
    },
  },
  {
    name:
      "numeric minimum Chrome requirements within the reference are preserved",
    fn() {
      assertEquals(
        checkChromeVersionRequirement(manifest()),
        null,
        "no minimum imposes no extra version gate",
      );
      for (
        const minimum of [
          "1",
          "130.0",
          "154.99999.0.0",
          "155",
          "155.0",
          "155.0.8059",
          "155.0.8059.84",
        ]
      ) {
        const input = manifest(minimum);
        assert(validateManifest(input), `${minimum}: valid manifest structure`);
        assertEquals(
          checkChromeVersionRequirement(input),
          null,
          `${minimum}: reference requirement satisfied`,
        );
        assertEquals(
          transformManifest(input, id).minimum_chrome_version,
          minimum,
          "conversion preserves the declared Chrome requirement",
        );
      }
    },
  },
  {
    name: "future Chrome minima use the evaluated Chrome reference, not Gecko",
    fn() {
      for (const minimum of ["155.0.8059.85", "155.1", "156", "157", "200.0"]) {
        const input = manifest(minimum);
        assert(
          validateManifest(input),
          "a future requirement is a valid manifest with an unsupported baseline",
        );
        const issue = checkChromeVersionRequirement(input);
        assert(issue !== null, "a future Chrome requirement must be rejected");
        assert(
          issue.includes(minimum) &&
            issue.includes(STORE_DOWNLOAD_CLIENT_VERSION),
          "version rejection identifies the requested and evaluated Chrome builds",
        );
        assert(
          checkCompatibility(input).includes(issue),
          "preflight exposes the same version boundary",
        );
      }
    },
  },
  {
    name: "malformed minimum Chrome requirements are invalid manifests",
    fn() {
      for (
        const minimum of [
          "",
          null,
          155,
          "155beta",
          "155..0",
          "155.0.0.0.1",
          "+155",
          "155.0.-1",
          "4294967296",
        ]
      ) {
        const input = manifest(minimum);
        assertEquals(
          validateManifest(input),
          false,
          `${String(minimum)}: malformed Chrome version`,
        );
        assert(
          checkChromeVersionRequirement(input)?.includes("numeric"),
          "malformed version receives a concrete diagnostic",
        );
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("ChromeVersionTarget.test.mts", tests);
}
