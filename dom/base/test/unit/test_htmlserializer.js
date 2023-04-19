/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testAttrsOutsideBodyWithRawBodyOnly() {
  // Create a simple HTML document in which the header contains a tag with set
  // attributes
  const htmlString = `<html><head><link rel="stylesheet" href="foo"/></head><body>some content</body></html>`;

  const parser = new DOMParser();
  const doc = parser.parseFromString(htmlString, "text/html");

  // Sanity check
  const linkElems = doc.head.getElementsByTagName("link");
  Assert.equal(
    linkElems.length,
    1,
    "document header should contain one link element"
  );
  Assert.equal(
    linkElems[0].rel,
    "stylesheet",
    "link element should have rel attribute set"
  );

  // Verify that the combination of raw output and body-only does not allow
  // attributes from header elements to creep into the output string
  const encoder = Cu.createDocumentEncoder("text/html");
  encoder.init(
    doc,
    "text/html",
    Ci.nsIDocumentEncoder.OutputRaw | Ci.nsIDocumentEncoder.OutputBodyOnly
  );

  const result = encoder.encodeToString();
  Assert.equal(
    result,
    "<body>some content</body>",
    "output should not contain attributes from head elements"
  );
});

add_task(async function testAttrsInsideBodyWithRawBodyOnly() {
  // Create a simple HTML document in which the body contains a tag with set
  // attributes
  const htmlString = `<html><head><link rel="stylesheet" href="foo"/></head><body><span id="foo">some content</span></body></html>`;

  const parser = new DOMParser();
  const doc = parser.parseFromString(htmlString, "text/html");

  // Sanity check
  const spanElem = doc.getElementById("foo");
  Assert.ok(spanElem, "should be able to get span element by ID");

  // Verify that the combination of raw output and body-only does not strip
  // tag attributes inside the body
  const encoder = Cu.createDocumentEncoder("text/html");
  encoder.init(
    doc,
    "text/html",
    Ci.nsIDocumentEncoder.OutputRaw | Ci.nsIDocumentEncoder.OutputBodyOnly
  );

  const result = encoder.encodeToString();
  Assert.equal(
    result,
    `<body><span id="foo">some content</span></body>`,
    "output should not contain attributes from head elements"
  );
});
