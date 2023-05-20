/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const snippet1 = `
  <style>
  #container {
    width: 150px;
    height: 150px;
    background: lightblue;
  }
  .hidden {
    content-visibility: hidden;
  }
  .auto {
    content-visibility: auto;
  }
  </style>
  <div id="container">
    <div class="hidden" id="hidden-target">
      hidden target
      <div id="hidden-child">
        hidden child
      </div>
    </div>
    <div class="auto" id="auto-target">
      auto target
      <div id="auto-child">
        auto child
      </div>
    </div>
  </div>
  `;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.content-visibility.enabled", true]],
  });
});

// Check if the element specified with `content-visibility` property is accessible
addAccessibleTask(
  snippet1,
  async function (browser, accDoc) {
    const container = findAccessibleChildByID(accDoc, "container");
    ok(
      findAccessibleChildByID(container, "hidden-target"),
      "hidden-target is accessible"
    );
    ok(
      findAccessibleChildByID(container, "auto-target"),
      "auto-target is accessible"
    );

    // The test checks if the child element of the element specified with
    // `content-visibility: hidden` is ignored from a11y tree
    let target = findAccessibleChildByID(accDoc, "hidden-target");
    ok(
      !findAccessibleChildByID(target, "hidden-child"),
      "Children of hidden-target is not accessible"
    );

    // The test checks if the child element of the element specified with
    // `content-visibility: auto` is showen in a11y tree
    target = findAccessibleChildByID(accDoc, "auto-target");
    ok(
      findAccessibleChildByID(target, "auto-child"),
      "Children of auto-target is accessible"
    );
  },
  { iframe: true, remoteIframe: true, chrome: true }
);

// Check if the element having `display: contents` and a child of `content-visibility: hidden` element isn't accessible
const snippet2 = `
  <style>
  #target {
    width: 150px;
    height: 150px;
    background-color: lightblue;
  }
  #child {
    width: 100px;
    height: 100px;
    background-color: lightgreen;
  }
  #grandchild {
    width: 50px;
    height: 50px;
    background-color: red;
  }
  .hidden {
    content-visibility: hidden;
  }
  .display-contents {
    display: contents;
  }
  </style>
  <div id="target" class="hidden">
    <div id="child" class="display-contents">
      <div id="grandchild"></div>
    </div>
  </div>
  `;

addAccessibleTask(
  snippet2,
  async function (browser, accDoc) {
    const target = findAccessibleChildByID(accDoc, "target");
    ok(
      !findAccessibleChildByID(target, "child"),
      "Element having `display: contents` and a child of `content-visibility: hidden` element isn't accessible"
    );
    testAccessibleTree(target, { SECTION: [] });
  },
  { iframe: true, remoteIframe: true, chrome: true }
);
