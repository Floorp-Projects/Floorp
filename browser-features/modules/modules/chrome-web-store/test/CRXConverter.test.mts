// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  CONVERSION_REPORT_PATH,
  CRXConverterClass,
  ORIGINAL_MANIFEST_PATH,
} from "../CRXConverter.sys.mts";
import { unzipSync, zipSync } from "fflate/browser";
import { MAX_FILE_SIZE } from "../Constants.sys.mts";
import {
  createSignedCRX2,
  createSignedCRX3,
  CRX_ECDSA_TEST_ID,
  CRX_TEST_ID,
} from "./helpers/CRXFixtures.mts";
import type { ChromeConversionReport, ChromeManifest } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const chromeId = CRX_TEST_ID;
const metadata = {
  id: chromeId,
  name: "Compatibility fixture",
  version: "1.0",
};
const encoder = new TextEncoder();
const decoder = new TextDecoder();

async function makeCRX(
  files: Record<string, Uint8Array>,
): Promise<ArrayBuffer> {
  return await createSignedCRX2(zipSync(files));
}

const originalManifest: ChromeManifest = {
  manifest_version: 3,
  name: metadata.name,
  version: metadata.version,
  permissions: [
    "scripting",
    "offscreen",
    "sidePanel",
    "tabCapture",
    "desktopCapture",
  ],
  background: { service_worker: "worker.js" },
  side_panel: { default_path: "panel.html" },
};

const tests: TestCase[] = [
  {
    name:
      "conversion checks minimum Chrome version against the frozen reference",
    async fn() {
      for (const minimum of ["155.0.8059.84", "155.0.8059.85", "invalid"]) {
        const result = await new CRXConverterClass().convert(
          await makeCRX({
            "manifest.json": encoder.encode(JSON.stringify({
              ...originalManifest,
              minimum_chrome_version: minimum,
            })),
          }),
          chromeId,
          metadata,
        );
        if (minimum === "155.0.8059.84") {
          assert(
            result.success && result.xpiData,
            "reference version accepted",
          );
          const output = unzipSync(new Uint8Array(result.xpiData));
          const manifest = JSON.parse(
            decoder.decode(output["manifest.json"]),
          ) as ChromeManifest;
          assertEquals(
            manifest.minimum_chrome_version,
            minimum,
            "original Chrome minimum remains visible",
          );
        } else {
          assertEquals(
            result.success,
            false,
            "unevaluated requirement rejected",
          );
          assertEquals(
            result.error?.code,
            minimum === "invalid" ? "INVALID_MANIFEST" : "VALIDATION_ERROR",
            "version validation failure classified",
          );
          if (minimum !== "invalid") {
            assert(
              result.error?.message.includes(minimum),
              "error identifies required Chrome version",
            );
          }
        }
      }
    },
  },
  {
    name: "conversion retains source compatibility warnings in its report",
    async fn() {
      const result = await new CRXConverterClass().convert(
        await makeCRX({
          "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
          "worker.js": encoder.encode(
            "console.log(chrome.sidePanel, chrome.declarativeNetRequest.MAX_NUMBER_OF_DYNAMIC_RULES); chrome.sidePanel.open();",
          ),
        }),
        chromeId,
        metadata,
      );
      assert(
        result.success && result.xpiData,
        "partial APIs remain installable",
      );
      const output = unzipSync(new Uint8Array(result.xpiData));
      const report = JSON.parse(
        decoder.decode(output[CONVERSION_REPORT_PATH]),
      ) as ChromeConversionReport;
      for (const issue of ["chrome.sidePanel", "Dynamic rules limit"]) {
        assert(
          report.warnings.some((warning) =>
            warning.includes("worker.js:") && warning.includes(issue)
          ),
          `source warning retained: ${issue}`,
        );
      }
      assertEquals(
        JSON.stringify(result.warnings),
        JSON.stringify(report.warnings),
        "visible conversion result and saved report agree",
      );
    },
  },
  {
    name:
      "conversion reports legacy DNR migration without changing its constraint",
    async fn() {
      const manifest = {
        ...originalManifest,
        declarative_net_request: {
          rule_resources: [{ id: "rules", enabled: true, path: "rules.json" }],
        },
      };
      const result = await new CRXConverterClass().convert(
        await makeCRX({
          "manifest.json": encoder.encode(JSON.stringify(manifest)),
          "rules.json": encoder.encode(
            JSON.stringify([
              {
                id: 1,
                action: { type: "block" },
                condition: { domains: ["example.com"] },
              },
            ]),
          ),
        }),
        chromeId,
        metadata,
      );
      assert(
        result.success && result.xpiData,
        "semantics-preserving alias should convert",
      );
      const output = unzipSync(new Uint8Array(result.xpiData));
      const rules = JSON.parse(decoder.decode(output["rules.json"])) as Array<{
        condition: Record<string, unknown>;
      }>;
      assertEquals(
        JSON.stringify(rules[0].condition),
        '{"initiatorDomains":["example.com"]}',
        "only field name migrates",
      );
      assert(
        result.warnings?.some(
          (warning) =>
            warning.includes("rules.json") &&
            warning.includes("initiatorDomains"),
        ),
        "migration reported",
      );
    },
  },
  {
    name:
      "conversion rejects invalid DNR instead of broadening a rule or copying an invalid ruleset",
    async fn() {
      const manifest = {
        ...originalManifest,
        declarative_net_request: {
          rule_resources: [{ id: "rules", enabled: true, path: "rules.json" }],
        },
      };
      for (
        const rules of [
          [
            {
              id: 1,
              action: { type: "block" },
              condition: { excludedRequestDomains: ["invalid domain"] },
            },
          ],
          { invalid: "not an array" },
          "malformed JSON",
        ]
      ) {
        const result = await new CRXConverterClass().convert(
          await makeCRX({
            "manifest.json": encoder.encode(JSON.stringify(manifest)),
            "rules.json": encoder.encode(
              typeof rules === "string" ? rules : JSON.stringify(rules),
            ),
          }),
          chromeId,
          metadata,
        );
        assertEquals(
          result.success,
          false,
          "incompatible rules prevent installation",
        );
        assertEquals(
          result.error?.code,
          "VALIDATION_ERROR",
          "ruleset validation failure",
        );
        assert(
          result.error?.message.includes("rules.json"),
          "error identifies ruleset",
        );
      }
    },
  },
  {
    name:
      "conversion preserves provenance and reports actual compatibility losses",
    async fn() {
      const manifestSource = JSON.stringify(originalManifest, null, 4) + "\n";
      const worker = encoder.encode("globalThis.started = true;");
      const result = await new CRXConverterClass({
        minGeckoVersion: "157.0",
      }).convert(
        await makeCRX({
          "manifest.json": encoder.encode(manifestSource),
          "worker.js": worker,
        }),
        chromeId,
        metadata,
      );
      assert(result.success && result.xpiData, "fixture should convert");
      const output = unzipSync(new Uint8Array(result.xpiData));
      const manifest = JSON.parse(
        decoder.decode(output["manifest.json"]),
      ) as ChromeManifest;
      const report = JSON.parse(
        decoder.decode(output[CONVERSION_REPORT_PATH]),
      ) as ChromeConversionReport;
      assertEquals(
        decoder.decode(output[ORIGINAL_MANIFEST_PATH]),
        manifestSource,
        "original manifest bytes retained",
      );
      assertEquals(
        decoder.decode(output["worker.js"]),
        decoder.decode(worker),
        "worker source retained",
      );
      assertEquals(
        manifest.browser_specific_settings?.gecko?.id,
        `${chromeId}@chromium-extension`,
        "stable internal ID",
      );
      assertEquals(
        manifest.browser_specific_settings?.gecko?.strict_min_version,
        "157.0",
        "configured minimum version",
      );
      assertEquals(
        manifest.browser_specific_settings?.gecko?.floorp_chrome_extension
          ?.original_id,
        chromeId,
        "explicit Chrome mode",
      );
      assert(
        manifest.permissions?.includes("offscreen"),
        "native offscreen permission retained",
      );
      assert(
        manifest.permissions?.includes("sidePanel"),
        "native sidePanel permission retained",
      );
      assert(
        manifest.permissions?.includes("tabCapture"),
        "native video capture permission retained",
      );
      assertEquals(
        manifest.background?.type,
        undefined,
        "classic worker retains default script type",
      );
      assertEquals(
        manifest.background?.scripts?.join(","),
        "worker.js",
        "native APIs need no injected scripts",
      );
      assertEquals(
        Object.keys(output).some((name) =>
          name.startsWith("__floorp_polyfills__/")
        ),
        false,
        "no synthetic polyfills",
      );
      assertEquals(report.chromeId, chromeId, "report records original ID");
      assertEquals(report.signature?.format, 2, "verified CRX format reported");
      assertEquals(
        report.signature?.algorithm,
        "rsa-sha1",
        "verified signature algorithm reported",
      );
      assert(
        /^[a-f0-9]{64}$/.test(report.signature?.publicKeySha256 ?? ""),
        "full signing key fingerprint is retained",
      );
      const idHash = Array.from(
        chromeId,
        (character) => (character.charCodeAt(0) - 97).toString(16),
      ).join("");
      assertEquals(
        report.signature?.publicKeySha256.slice(0, 32),
        idHash,
        "reported signing key fingerprint binds the Chrome ID",
      );
      assertEquals(
        report.originalVersion,
        metadata.version,
        "report records source version",
      );
      assert(
        report.removedPermissions.includes("desktopCapture"),
        "capture limitation reported",
      );
      assert(
        report.warnings.some((warning) => warning.includes("desktopCapture")),
        "permission loss surfaced to caller",
      );
      assert(
        report.warnings.some((warning) => warning.includes("ervice worker")),
        "worker fallback surfaced to caller",
      );
      assertEquals(
        JSON.stringify(report.warnings),
        JSON.stringify(result.warnings),
        "archive and caller share diagnostics",
      );
      assertEquals(
        JSON.stringify(report),
        JSON.stringify(result.report),
        "report returned for management clients",
      );
    },
  },
  {
    name:
      "conversion authenticates CRX3 RSA and ECDSA packages before preserving signature metadata",
    async fn() {
      const zip = zipSync({
        "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
      });
      for (const algorithm of ["rsa", "ecdsa"] as const) {
        const id = algorithm === "rsa" ? CRX_TEST_ID : CRX_ECDSA_TEST_ID;
        const result = await new CRXConverterClass().convert(
          await createSignedCRX3(zip, { algorithm }),
          id,
          { ...metadata, id },
        );
        assert(
          result.success && result.xpiData,
          "authenticated CRX3 should convert",
        );
        const output = unzipSync(new Uint8Array(result.xpiData));
        const report = JSON.parse(
          decoder.decode(output[CONVERSION_REPORT_PATH]),
        ) as ChromeConversionReport;
        assertEquals(report.signature?.format, 3, "CRX3 format retained");
        assertEquals(
          report.signature?.algorithm,
          `${algorithm}-sha256`,
          "verified algorithm retained",
        );
        assertEquals(report.chromeId, id, "ID corresponds to the verified key");
        assertEquals(
          JSON.stringify(report),
          JSON.stringify(result.report),
          "archive and caller share authenticated report",
        );
      }
    },
  },
  {
    name:
      "conversion rejects signed payload tampering before attempting ZIP extraction",
    async fn() {
      const zip = zipSync({
        "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
      });
      for (
        const crx of [
          await createSignedCRX2(zip),
          await createSignedCRX3(zip),
        ]
      ) {
        const bytes = new Uint8Array(crx);
        bytes[bytes.length - 1] ^= 1;
        const result = await new CRXConverterClass().convert(
          crx,
          chromeId,
          metadata,
        );
        assertEquals(result.success, false, "tampered signed package rejected");
        assertEquals(
          result.error?.code,
          "INVALID_CRX",
          "authentication fails before ZIP processing",
        );
        assertEquals(
          result.xpiData,
          undefined,
          "no unauthenticated XPI returned",
        );
        assertEquals(
          result.report,
          undefined,
          "no authenticated report returned",
        );
      }
    },
  },
  {
    name:
      "conversion rejects a genuine signed package requested under another valid Chrome ID",
    async fn() {
      const otherId = "a".repeat(32);
      const result = await new CRXConverterClass().convert(
        await makeCRX({
          "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
        }),
        otherId,
        { ...metadata, id: otherId },
      );
      assertEquals(result.success, false, "requested ID must match signer");
      assertEquals(
        result.error?.code,
        "INVALID_CRX",
        "signature identity error exposed",
      );
      assertEquals(
        result.xpiData,
        undefined,
        "no package with spoofed original ID",
      );
    },
  },
  {
    name: "conversion rejects a CRX3 signer declaring another original ID",
    async fn() {
      const otherId = "a".repeat(32);
      const result = await new CRXConverterClass().convert(
        await createSignedCRX3(
          zipSync({
            "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
          }),
          {
            declaredChromeId: otherId,
          },
        ),
        otherId,
        { ...metadata, id: otherId },
      );
      assertEquals(
        result.success,
        false,
        "signed original ID also requires matching developer key",
      );
      assertEquals(
        result.error?.code,
        "INVALID_CRX",
        "spoofed declaration rejected before conversion",
      );
    },
  },
  {
    name:
      "conversion rejects a signed ZIP advertising oversized expansion before allocation",
    async fn() {
      for (const advertisedSize of [MAX_FILE_SIZE + 1, 0xffffffff]) {
        const zip = Uint8Array.from(
          zipSync({
            "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
            "inflation.bin": new Uint8Array([1]),
          }),
        );
        const view = new DataView(zip.buffer);
        let changed = false;
        for (let offset = 0; offset + 46 <= zip.length; offset++) {
          if (view.getUint32(offset, true) !== 0x02014b50) continue;
          const nameLength = view.getUint16(offset + 28, true);
          const name = decoder.decode(
            zip.subarray(offset + 46, offset + 46 + nameLength),
          );
          if (name !== "inflation.bin") continue;
          view.setUint32(offset + 24, advertisedSize, true);
          changed = true;
          break;
        }
        assert(
          changed,
          "fixture central directory advertises expanded entry size",
        );
        const result = await new CRXConverterClass().convert(
          await createSignedCRX3(zip),
          chromeId,
          metadata,
        );
        assertEquals(
          result.success,
          false,
          "oversized authenticated archive rejected",
        );
        assertEquals(
          result.error?.code,
          "VALIDATION_ERROR",
          "archive expansion limit error precedes allocation",
        );
        assert(
          result.error?.message.includes("expansion limits"),
          "specific expansion rejection",
        );
        assertEquals(
          result.xpiData,
          undefined,
          "no oversized converted package returned",
        );
      }
    },
  },
  {
    name: "conversion rejects bare ZIP input without a package proof",
    async fn() {
      const zip = Uint8Array.from(
        zipSync({
          "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
        }),
      );
      const result = await new CRXConverterClass().convert(
        zip.buffer,
        chromeId,
        metadata,
      );
      assertEquals(
        result.success,
        false,
        "bare ZIP cannot establish original identity",
      );
      assertEquals(
        result.error?.code,
        "INVALID_CRX",
        "authenticated CRX input required",
      );
    },
  },
  {
    name:
      "conversion rejects a package that could overwrite conversion provenance",
    async fn() {
      const result = await new CRXConverterClass().convert(
        await makeCRX({
          "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
          [CONVERSION_REPORT_PATH]: encoder.encode('{"warnings":[]}'),
        }),
        chromeId,
        metadata,
      );
      assertEquals(result.success, false, "reserved report directory rejected");
      assertEquals(
        result.error?.code,
        "VALIDATION_ERROR",
        "validation error exposed",
      );
    },
  },
  {
    name: "conversion rejects IDs outside the Chrome alphabet",
    async fn() {
      const result = await new CRXConverterClass().convert(
        await makeCRX({
          "manifest.json": encoder.encode(JSON.stringify(originalManifest)),
        }),
        "z".repeat(32),
        metadata,
      );
      assertEquals(result.success, false, "invalid original ID rejected");
      assertEquals(
        result.error?.code,
        "VALIDATION_ERROR",
        "invalid ID error exposed",
      );
    },
  },
  // --- Constructor / options ---
  {
    name: "CRXConverterClass can be instantiated with default options",
    fn() {
      const converter = new CRXConverterClass();
      assert(converter !== null, "should create instance");
    },
  },
  {
    name: "CRXConverterClass accepts custom options",
    fn() {
      const converter = new CRXConverterClass({
        validateCode: false,
        sanitizeDNR: false,
        minGeckoVersion: "120.0",
      });
      assert(converter !== null, "should create instance with custom options");
    },
  },
  {
    name: "CRXConverterClass accepts partial options",
    fn() {
      const converter = new CRXConverterClass({ validateCode: false });
      assert(converter !== null, "should create instance with partial options");
    },
  },

  // --- convert error handling ---
  {
    name: "convert returns failure for empty ArrayBuffer",
    async fn() {
      const converter = new CRXConverterClass();
      const emptyBuffer = new ArrayBuffer(0);
      const result = await converter.convert(emptyBuffer, "test-id", {
        id: "test-id",
        name: "Test Extension",
        version: "1.0",
      });
      assertEquals(result.success, false, "should fail for empty buffer");
      assert(result.error !== undefined, "should have an error");
    },
  },
  {
    name: "convert returns failure for random garbage data",
    async fn() {
      const converter = new CRXConverterClass();
      const garbage = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]).buffer;
      const result = await converter.convert(garbage, "test-id", {
        id: "test-id",
        name: "Test",
        version: "1.0",
      });
      assertEquals(result.success, false, "should fail for garbage data");
      assert(result.error !== undefined, "should have an error");
    },
  },
  {
    name: "convert returns failure for truncated CRX header",
    async fn() {
      const converter = new CRXConverterClass();
      // CRX3 magic: Cr24 = 0x43723234
      const header = new Uint8Array([
        0x43,
        0x72,
        0x32,
        0x34,
        0x03,
        0x00,
        0x00,
        0x00,
      ]);
      const result = await converter.convert(header.buffer, "test-id", {
        id: "test-id",
        name: "Test",
        version: "1.0",
      });
      assertEquals(result.success, false, "should fail for truncated CRX");
    },
  },

  // --- ConversionResult shape ---
  {
    name: "failed conversion result has expected shape",
    async fn() {
      const converter = new CRXConverterClass();
      const result = await converter.convert(new ArrayBuffer(0), "x", {
        id: "x",
        name: "X",
        version: "0",
      });
      assert("success" in result, "should have success property");
      assert("warnings" in result, "should have warnings property");
      assert(Array.isArray(result.warnings), "warnings should be an array");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("CRXConverter.test.mts", tests);
}
