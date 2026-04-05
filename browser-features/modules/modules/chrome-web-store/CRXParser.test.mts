// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  parseCRXHeader,
  extractZipFromCRX,
  isValidCRX,
} from "./CRXParser.sys.mts";
import {
  CRX_MAGIC,
  CRX2_VERSION,
  CRX3_VERSION,
  ZIP_SIGNATURE,
} from "./Constants.sys.mts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers — construct CRX buffers
// ---------------------------------------------------------------------------

function writeMagic(view: DataView): void {
  for (let i = 0; i < CRX_MAGIC.length; i++) {
    view.setUint8(i, CRX_MAGIC.charCodeAt(i));
  }
}

function writeZipSignature(view: DataView, offset: number): void {
  view.setUint32(offset, ZIP_SIGNATURE, true);
}

/** Build a minimal CRX2 buffer: magic + version + pubkeyLen + sigLen + [pubkey] + [sig] + [zip?] */
function buildCRX2(
  pubkeyLen: number,
  sigLen: number,
  appendZip: boolean,
): ArrayBuffer {
  const zipSize = appendZip ? 4 : 0;
  const totalSize = 16 + pubkeyLen + sigLen + zipSize;
  const buf = new ArrayBuffer(totalSize);
  const view = new DataView(buf);

  writeMagic(view);
  view.setUint32(4, CRX2_VERSION, true);
  view.setUint32(8, pubkeyLen, true);
  view.setUint32(12, sigLen, true);

  if (appendZip) {
    writeZipSignature(view, 16 + pubkeyLen + sigLen);
  }

  return buf;
}

/** Build a minimal CRX3 buffer: magic + version + headerLen + [header] + [zip?] */
function buildCRX3(headerLen: number, appendZip: boolean): ArrayBuffer {
  const zipSize = appendZip ? 4 : 0;
  const totalSize = 12 + headerLen + zipSize;
  const buf = new ArrayBuffer(totalSize);
  const view = new DataView(buf);

  writeMagic(view);
  view.setUint32(4, CRX3_VERSION, true);
  view.setUint32(8, headerLen, true);

  if (appendZip) {
    writeZipSignature(view, 12 + headerLen);
  }

  return buf;
}

// ---------------------------------------------------------------------------
// Tests — parseCRXHeader
// ---------------------------------------------------------------------------

function testParseCRX2HeaderValid(): void {
  const buf = buildCRX2(10, 20, false);
  const header = parseCRXHeader(buf);
  assert(header !== null, "CRX2 header should not be null");
  assertEquals(header.version, 2, "version should be 2");
  assertEquals(header.publicKeyLength, 10, "publicKeyLength should be 10");
  assertEquals(header.signatureLength, 20, "signatureLength should be 20");
  assertEquals(header.zipOffset, 46, "zipOffset should be 16+10+20=46");
}

function testParseCRX3HeaderValid(): void {
  const buf = buildCRX3(100, false);
  const header = parseCRXHeader(buf);
  assert(header !== null, "CRX3 header should not be null");
  assertEquals(header.version, 3, "version should be 3");
  assertEquals(header.headerLength, 100, "headerLength should be 100");
  assertEquals(header.zipOffset, 112, "zipOffset should be 12+100=112");
}

function testParseCRXHeaderTooSmall(): void {
  const buf = new ArrayBuffer(8);
  const header = parseCRXHeader(buf);
  assertEquals(header, null, "should return null for buffer < 16 bytes");
}

function testParseCRXHeaderBadMagic(): void {
  const buf = new ArrayBuffer(16);
  const view = new DataView(buf);
  // Write wrong magic "XXXX"
  view.setUint8(0, 0x58);
  view.setUint8(1, 0x58);
  view.setUint8(2, 0x58);
  view.setUint8(3, 0x58);
  view.setUint32(4, CRX2_VERSION, true);
  const header = parseCRXHeader(buf);
  assertEquals(header, null, "should return null for bad magic");
}

function testParseCRXHeaderUnknownVersion(): void {
  const buf = new ArrayBuffer(16);
  const view = new DataView(buf);
  writeMagic(view);
  view.setUint32(4, 99, true); // unknown version
  const header = parseCRXHeader(buf);
  assertEquals(header, null, "should return null for unknown version");
}

// ---------------------------------------------------------------------------
// Tests — isValidCRX
// ---------------------------------------------------------------------------

function testIsValidCRXTrue(): void {
  const buf = buildCRX2(0, 0, false);
  assertEquals(isValidCRX(buf), true, "valid CRX2 should return true");
}

function testIsValidCRXFalse(): void {
  const buf = new ArrayBuffer(4);
  assertEquals(isValidCRX(buf), false, "garbage buffer should return false");
}

// ---------------------------------------------------------------------------
// Tests — extractZipFromCRX
// ---------------------------------------------------------------------------

function testExtractZipFromCRX2(): void {
  const buf = buildCRX2(5, 10, true);
  const zip = extractZipFromCRX(buf);
  assert(zip !== null, "should extract zip from CRX2");
  assertEquals(zip.byteLength, 4, "zip should be 4 bytes (signature only)");
  const sig = new DataView(zip).getUint32(0, true);
  assertEquals(sig, ZIP_SIGNATURE, "extracted data should start with PK sig");
}

function testExtractZipFromCRX3(): void {
  const buf = buildCRX3(20, true);
  const zip = extractZipFromCRX(buf);
  assert(zip !== null, "should extract zip from CRX3");
  const sig = new DataView(zip).getUint32(0, true);
  assertEquals(sig, ZIP_SIGNATURE, "extracted data should start with PK sig");
}

function testExtractZipInvalidCRX(): void {
  const buf = new ArrayBuffer(4);
  const zip = extractZipFromCRX(buf);
  assertEquals(zip, null, "should return null for invalid CRX");
}

function testExtractZipMissingZipSignature(): void {
  // Valid CRX2 header but no ZIP signature at offset
  const buf = buildCRX2(0, 0, false);
  const zip = extractZipFromCRX(buf);
  assertEquals(zip, null, "should return null when no ZIP signature present");
}

function testCRX2ZipOffsetCalculation(): void {
  const pubkeyLen = 32;
  const sigLen = 64;
  const header = parseCRXHeader(buildCRX2(pubkeyLen, sigLen, false));
  assert(header !== null, "header should not be null");
  assertEquals(
    header.zipOffset,
    16 + pubkeyLen + sigLen,
    "CRX2 zipOffset = 16 + pubkeyLen + sigLen",
  );
}

function testCRX3ZipOffsetCalculation(): void {
  const headerLen = 256;
  const header = parseCRXHeader(buildCRX3(headerLen, false));
  assert(header !== null, "header should not be null");
  assertEquals(
    header.zipOffset,
    12 + headerLen,
    "CRX3 zipOffset = 12 + headerLength",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "parse CRX2 header valid", fn: testParseCRX2HeaderValid },
    { name: "parse CRX3 header valid", fn: testParseCRX3HeaderValid },
    { name: "parse header too small", fn: testParseCRXHeaderTooSmall },
    { name: "parse header bad magic", fn: testParseCRXHeaderBadMagic },
    {
      name: "parse header unknown version",
      fn: testParseCRXHeaderUnknownVersion,
    },
    { name: "isValidCRX true", fn: testIsValidCRXTrue },
    { name: "isValidCRX false", fn: testIsValidCRXFalse },
    { name: "extract zip from CRX2", fn: testExtractZipFromCRX2 },
    { name: "extract zip from CRX3", fn: testExtractZipFromCRX3 },
    { name: "extract zip invalid CRX", fn: testExtractZipInvalidCRX },
    {
      name: "extract zip missing ZIP signature",
      fn: testExtractZipMissingZipSignature,
    },
    { name: "CRX2 zip offset calculation", fn: testCRX2ZipOffsetCalculation },
    { name: "CRX3 zip offset calculation", fn: testCRX3ZipOffsetCalculation },
  ];

  await runTests("CRXParser.test.mts", tests);
}
