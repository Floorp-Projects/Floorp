/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF =
  "devtools.testing.bypass-walker-children-iframe-guard";

add_task(async function testIframe() {
  info("Check that dedicated walker is used for retrieving iframe children");

  const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(`
  <h1>Test iframe</h1>
  <iframe src="https://example.com/document-builder.sjs?html=Hello"></iframe>
`)}`;

  const { walker } = await initInspectorFront(TEST_URI);
  const iframeNodeFront = await walker.querySelector(walker.rootNode, "iframe");

  is(
    iframeNodeFront.useChildTargetToFetchChildren,
    isEveryFrameTargetEnabled(),
    "useChildTargetToFetchChildren has expected value"
  );
  is(
    iframeNodeFront.numChildren,
    1,
    "numChildren is set to 1 (for the #document node)"
  );

  const res = await walker.children(iframeNodeFront);
  is(
    res.nodes.length,
    1,
    "Retrieving the iframe children return an array with one element"
  );
  const documentNodeFront = res.nodes[0];
  is(
    documentNodeFront.nodeName,
    "#document",
    "The child is the #document element"
  );
  if (isEveryFrameTargetEnabled()) {
    ok(
      documentNodeFront.walkerFront !== walker,
      "The child walker is different from the top level document one when EFT is enabled"
    );
  }
  is(
    documentNodeFront.parentNode(),
    iframeNodeFront,
    "The child parent was set to the original iframe nodeFront"
  );
});

add_task(async function testIframeBlockedByCSP() {
  info("Check that iframe blocked by CSP don't have any children");

  const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(`
  <h1>Test CSP-blocked iframe</h1>
  <iframe src="https://example.org/document-builder.sjs?html=Hello"></iframe>
`)}&headers=content-security-policy:default-src 'self'`;

  const { walker } = await initInspectorFront(TEST_URI);
  const iframeNodeFront = await walker.querySelector(walker.rootNode, "iframe");

  is(
    iframeNodeFront.useChildTargetToFetchChildren,
    false,
    "useChildTargetToFetchChildren is false"
  );
  is(iframeNodeFront.numChildren, 0, "numChildren is set to 0");

  info("Test calling WalkerFront#children with the safe guard removed");
  await pushPref(BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF, true);

  let res = await walker.children(iframeNodeFront);
  is(
    res.nodes.length,
    0,
    "Retrieving the iframe children return an empty array"
  );

  info("Test calling WalkerFront#children again, but with the safe guard");
  Services.prefs.clearUserPref(BYPASS_WALKERFRONT_CHILDREN_IFRAME_GUARD_PREF);
  res = await walker.children(iframeNodeFront);
  is(
    res.nodes.length,
    0,
    "Retrieving the iframe children return an empty array"
  );
});
