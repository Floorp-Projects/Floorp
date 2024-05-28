/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Test web area role and AXLoadComplete event
addAccessibleTask(``, async browser => {
  let evt = waitForMacEvent("AXLoadComplete", iface => {
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
addAccessibleTask(`<title>webarea test</title>`, async browser => {
  // If the iframe loads before the top level document finishes loading, we'll
  // get both an AXLayoutComplete event for the iframe and an AXLoadComplete
  // event for the document. Otherwise, if the iframe loads after the
  // document, we'll get one AXLoadComplete event.
  let eventPromise = Promise.race([
    waitForMacEvent("AXLayoutComplete", iface => {
      return iface.getAttributeValue("AXDescription") == "iframe document";
    }),
    waitForMacEvent("AXLoadComplete", iface => {
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

// Test AXContents in outer doc (AXScrollArea)
addAccessibleTask(
  `<p id="p">Hello</p>`,
  async (browser, accDoc) => {
    const doc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    const outerDoc = doc.getAttributeValue("AXParent");
    const p = getNativeInterface(accDoc, "p");

    let contents = outerDoc.getAttributeValue("AXContents");
    is(contents.length, 1, "outer doc has single AXContents member");
    is(
      contents[0].getAttributeValue("AXRole"),
      "AXWebArea",
      "AXContents member is a web area"
    );

    ok(
      !doc.attributeNames.includes("AXContents"),
      "Web area does not have XContents"
    );
    ok(
      !p.attributeNames.includes("AXContents"),
      "Web content does not hace XContents"
    );
  },
  { iframe: true, remoteIframe: true }
);
