/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

async function runTest(setupFunc, expected) {
  let objectOutStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIObjectOutputStream
  );
  let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
  pipe.init(
    false /* non-blocking input */,
    false /* non-blocking output */,
    0 /* segment size */,
    0 /* max segments */
  );
  objectOutStream.setOutputStream(pipe.outputStream);

  setupFunc(objectOutStream);

  let objectInStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIObjectInputStream
  );
  objectInStream.setInputStream(pipe.inputStream);

  let referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY);
  try {
    referrerInfo.read(objectInStream);
  } catch (e) {
    Assert.ok(false, "Shouldn't fail when deserializing.");
    return;
  }

  Assert.ok(true, "Successfully deserialize the referrerInfo.");

  let { referrerPolicy, sendReferrer, computedReferrerSpec } = expected;
  Assert.equal(
    referrerInfo.referrerPolicy,
    referrerPolicy,
    "The referrerInfo has the expected referrer policy."
  );

  Assert.equal(
    referrerInfo.sendReferrer,
    sendReferrer,
    "The referrerInfo has the expected sendReferrer value."
  );

  if (computedReferrerSpec) {
    Assert.equal(
      referrerInfo.computedReferrerSpec,
      computedReferrerSpec,
      "The referrerInfo has the expected computedReferrerSpec value."
    );
  }
}

// Test deserializing referrer info with the old format.
add_task(async function test_deserializeOldReferrerInfo() {
  // Test with a regular old format.
  await runTest(
    stream => {
      // Write to the output stream with the old format.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.writeBoolean(false); // sendReferrer
      stream.writeBoolean(false); // isComputed
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: false,
    }
  );

  // Test with an old format with `sendReferrer` is true.
  await runTest(
    stream => {
      // Write to the output stream with the old format.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.writeBoolean(true); // sendReferrer
      stream.writeBoolean(false); // isComputed
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: true,
    }
  );

  // Test with an old format with a computed Referrer.
  await runTest(
    stream => {
      // Write to the output stream with the old format with a string.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.writeBoolean(false); // sendReferrer
      stream.writeBoolean(true); // isComputed
      stream.writeStringZ("https://example.com/"); // computedReferrer
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: false,
      computedReferrerSpec: "https://example.com/",
    }
  );

  // Test with an old format with a computed Referrer and sendReferrer as true.
  await runTest(
    stream => {
      // Write to the output stream with the old format with a string.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.writeBoolean(true); // sendReferrer
      stream.writeBoolean(true); // isComputed
      stream.writeStringZ("https://example.com/"); // computedReferrer
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: true,
      computedReferrerSpec: "https://example.com/",
    }
  );
});

// Test deserializing referrer info with the current format.
add_task(async function test_deserializeReferrerInfo() {
  // Test with a current format.
  await runTest(
    stream => {
      // Write to the output stream with the new format.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // original policy
      stream.writeBoolean(false); // sendReferrer
      stream.writeBoolean(false); // isComputed
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: false,
    }
  );

  // Test with a current format with sendReferrer as true.
  await runTest(
    stream => {
      // Write to the output stream with the new format.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // original policy
      stream.writeBoolean(true); // sendReferrer
      stream.writeBoolean(false); // isComputed
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: true,
    }
  );

  // Test with a current format with a computedReferrer.
  await runTest(
    stream => {
      // Write to the output stream with the new format with a string.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // original policy
      stream.writeBoolean(false); // sendReferrer
      stream.writeBoolean(true); // isComputed
      stream.writeStringZ("https://example.com/"); // computedReferrer
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: false,
      computedReferrerSpec: "https://example.com/",
    }
  );

  // Test with a current format with a computedReferrer and sendReferrer as true.
  await runTest(
    stream => {
      // Write to the output stream with the new format with a string.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // policy
      stream.write32(Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN); // original policy
      stream.writeBoolean(true); // sendReferrer
      stream.writeBoolean(true); // isComputed
      stream.writeStringZ("https://example.com/"); // computedReferrer
      stream.writeBoolean(true); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
      sendReferrer: true,
      computedReferrerSpec: "https://example.com/",
    }
  );

  // Test with a current format that the tailing bytes are all zero.
  await runTest(
    stream => {
      // Write to the output stream with the new format with a string.
      stream.writeBoolean(true); // nonNull
      stream.writeStringZ("https://example.com/"); // spec
      stream.write32(Ci.nsIReferrerInfo.EMPTY); // policy
      stream.write32(Ci.nsIReferrerInfo.EMPTY); // original policy
      stream.writeBoolean(false); // sendReferrer
      stream.writeBoolean(false); // isComputed
      stream.writeBoolean(false); // initialized
      stream.writeBoolean(false); // overridePolicyByDefault
    },
    {
      referrerPolicy: Ci.nsIReferrerInfo.EMPTY,
      sendReferrer: false,
    }
  );
});
