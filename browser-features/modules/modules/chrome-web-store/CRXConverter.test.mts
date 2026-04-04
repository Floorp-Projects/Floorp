// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CRXConverterClass } from "./CRXConverter.sys.mts";

type TestCase = { name: string; fn: () => void | Promise<void> };
function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}
function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

const tests: TestCase[] = [
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
      const header = new Uint8Array([0x43, 0x72, 0x32, 0x34, 0x03, 0x00, 0x00, 0x00]);
      const result = await converter.convert(header.buffer, "test-id", {
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
  for (const t of tests) {
    try {
      await t.fn();
      console.log(`[PASS] ${t.name}`);
    } catch (e) {
      console.error(`[FAIL] ${t.name}:`, e);
      throw e;
    }
  }
}
