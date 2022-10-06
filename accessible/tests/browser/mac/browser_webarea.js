/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Test web area role and AXLoadComplete event
addAccessibleTask(``, async (browser, accDoc) => {
  let evt = waitForMacEvent("AXLoadComplete", (iface, data) => {
    return iface.getAttributeValue("AXDescription") == "webarea test";
  });
  await SpecialPowers.spawn(browser, [], () => {
    content.location = "data:text/html,<title>webarea test</title>";
  });
  let doc = await evt;

  is(
    doc.getAttributeValue("AXRole"),
    "AXWebArea",
    "document has AXWebArea role"
  );
  is(doc.getAttributeValue("AXValue"), "", "document has no AXValue");
  is(doc.getAttributeValue("AXTitle"), null, "document has no AXTitle");

  is(doc.getAttributeValue("AXLoaded"), 1, "document has finished loading");
});

// Test iframe web area role and AXLayoutComplete event
addAccessibleTask(`<title>webarea test</title>`, async (browser, accDoc) => {
  // If the iframe loads before the top level document finishes loading, we'll
  // get both an AXLayoutComplete event for the iframe and an AXLoadComplete
  // event for the document. Otherwise, if the iframe loads after the
  // document, we'll get one AXLoadComplete event.
  let eventPromise = Promise.race([
    waitForMacEvent("AXLayoutComplete", (iface, data) => {
      return iface.getAttributeValue("AXDescription") == "iframe document";
    }),
    waitForMacEvent("AXLoadComplete", (iface, data) => {
      return iface.getAttributeValue("AXDescription") == "webarea test";
    }),
  ]);
  await SpecialPowers.spawn(browser, [], () => {
    const iframe = content.document.createElement("iframe");
    iframe.src = "data:text/html,<title>iframe document</title>hello world";
    content.document.body.appendChild(iframe);
  });
  let doc = await eventPromise;

  if (doc.getAttributeValue("AXTitle")) {
    // iframe should have no title, so if we get a title here
    // we've got the main document and need to get the iframe from
    // the main doc
    doc = doc.getAttributeValue("AXChildren")[0];
  }

  is(
    doc.getAttributeValue("AXRole"),
    "AXWebArea",
    "iframe document has AXWebArea role"
  );
  is(doc.getAttributeValue("AXValue"), "", "iframe document has no AXValue");
  is(doc.getAttributeValue("AXTitle"), null, "iframe document has no AXTitle");
  is(
    doc.getAttributeValue("AXDescription"),
    "iframe document",
    "test has correct label"
  );

  is(
    doc.getAttributeValue("AXLoaded"),
    1,
    "iframe document has finished loading"
  );
});
