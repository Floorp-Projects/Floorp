/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * bug 1673627 - Test that remote iframes with short inline content,
 * get their inner remote document displayed instead of the inlineTextChild content.
 */
const TEST_URI = `data:text/html,<div id="root"><iframe src="https://example.com/document-builder.sjs?html=<div id=com>com"><br></iframe></div>`;

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URI);
  const tree = `
    id="root"
      iframe
        #document
          html
            head
            body
              id="com"`;
  await assertMarkupViewAsTree(tree, "#root", inspector);
});

// Test regular remote frames
const FRAME_URL = `https://example.com/document-builder.sjs?html=<div>com`;
const TEST_REMOTE_FRAME = `https://example.org/document-builder.sjs?html=<div id="org-root"><iframe src="${FRAME_URL}"></iframe></div>`;
add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_REMOTE_FRAME);
  const tree = `
    id="org-root"
      iframe
        #document
          html
            head
            body
              com`;
  await assertMarkupViewAsTree(tree, "#org-root", inspector);
});
