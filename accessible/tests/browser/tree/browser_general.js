/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Verify adding `overflow:hidden;` styling to a div causes it to
 * get an accessible.
 */
addAccessibleTask(`<p>hello world</p>`, async function (browser, docAcc) {
  const originalTree = { DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }] };

  testAccessibleTree(docAcc, originalTree);
  info("Adding div element");
  await contentSpawnMutation(
    browser,
    { unexpected: [[EVENT_REORDER, docAcc]] },
    function () {
      const d = content.document.createElement("div");
      content.document.body.appendChild(d);
    }
  );

  testAccessibleTree(docAcc, originalTree);
  info("Adding overflow:hidden styling to div");
  await contentSpawnMutation(
    browser,
    { expected: [[EVENT_REORDER, docAcc]] },
    function () {
      content.document.body.lastElementChild.setAttribute(
        "style",
        "overflow:hidden;"
      );
    }
  );

  testAccessibleTree(docAcc, {
    DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }, { TEXT_CONTAINER: [] }],
  });
});

/**
 * Verify adding `overflow:scroll;` styling to a div causes
 * it to  get an accessible.
 */
addAccessibleTask(`<p>hello world</p>`, async function (browser, docAcc) {
  const originalTree = { DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }] };

  testAccessibleTree(docAcc, originalTree);
  info("Adding div element");
  await contentSpawnMutation(
    browser,
    { unexpected: [[EVENT_REORDER, docAcc]] },
    function () {
      const d = content.document.createElement("div");
      content.document.body.appendChild(d);
    }
  );

  testAccessibleTree(docAcc, originalTree);
  info("Adding overflow:scroll styling to div");
  await contentSpawnMutation(
    browser,
    { expected: [[EVENT_REORDER, docAcc]] },
    function () {
      content.document.body.lastElementChild.setAttribute(
        "style",
        "overflow:scroll;"
      );
    }
  );

  testAccessibleTree(docAcc, {
    DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }, { TEXT_CONTAINER: [] }],
  });
});

/**
 * Verify adding `overflow:auto;` styling to a div causes
 * it to get an accessible, but `overflow:visible` does not.
 */
addAccessibleTask(`<p>hello world</p>`, async function (browser, docAcc) {
  const originalTree = { DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }] };

  testAccessibleTree(docAcc, originalTree);
  info("Adding div element");
  await contentSpawnMutation(
    browser,
    { unexpected: [[EVENT_REORDER, docAcc]] },
    function () {
      const d = content.document.createElement("div");
      content.document.body.appendChild(d);
    }
  );

  testAccessibleTree(docAcc, originalTree);
  info("Adding overflow:visible styling to div");
  await contentSpawnMutation(
    browser,
    { unexpected: [[EVENT_REORDER, docAcc]] },
    function () {
      content.document.body.lastElementChild.setAttribute(
        "style",
        "overflow:visible;"
      );
    }
  );

  testAccessibleTree(docAcc, originalTree);
  info("Adding overflow:auto styling to div");
  await contentSpawnMutation(
    browser,
    { expected: [[EVENT_REORDER, docAcc]] },
    function () {
      content.document.body.lastElementChild.setAttribute(
        "style",
        "overflow:auto;"
      );
    }
  );

  testAccessibleTree(docAcc, {
    DOCUMENT: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }, { TEXT_CONTAINER: [] }],
  });
});
