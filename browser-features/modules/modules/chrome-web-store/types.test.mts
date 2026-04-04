// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CWSError, CWSErrorCode } from "./types.ts";

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals(actual, expected, message) {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

function testLocalizedMessages() {
  const samples = Object.values(CWSErrorCode);
  for (const code of samples) {
    const error = new CWSError(code, `raw-${code}`);
    const localized = error.getLocalizedMessage();
    assert(typeof localized === "string" && localized.length > 0, `localized message should exist for ${code}`);
  }

  const specific = new CWSError(CWSErrorCode.INVALID_CRX, "invalid");
  assertEquals(specific.getLocalizedMessage(), "無効なCRXファイルです", "INVALID_CRX localized string");
}

function testErrorShape() {
  const cause = new Error("network down");
  const error = new CWSError(CWSErrorCode.NETWORK_ERROR, "failed", cause);

  assert(error instanceof Error, "CWSError should inherit Error");
  assertEquals(error.name, "CWSError", "error name");
  assertEquals(error.code, CWSErrorCode.NETWORK_ERROR, "error code");

  const json = error.toJSON();
  assertEquals(json.code, CWSErrorCode.NETWORK_ERROR, "toJSON code");
  assertEquals(json.message, "failed", "toJSON message");
  assertEquals(json.cause, "network down", "toJSON cause message");
}

export function runAllTests() {
  testLocalizedMessages();
  testErrorShape();
}
