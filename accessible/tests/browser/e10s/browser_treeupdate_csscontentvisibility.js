/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const snippet = `
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
    #content-child {
      width: 100px;
      height: 100px;
      background-color: green;
      display: contents;
    }
    .hidden {
      content-visibility: hidden;
    }
    .auto {
      content-visibility: auto;
    }
    #hidden-subtree-2 {
      visibility: hidden;
    }
  </style>
  <div class="hidden" id="target">
    <div id="child">A</div>
    <div id="content-child">B</div>
    <div id="hidden-subtree-1" class="hidden">C</div>
    <div id="hidden-subtree-2">D</div>
    <div id="shadow-host"></div>
  </div>
  <script>
    const host = document.querySelector("#shadow-host");
    const shadowRoot = host.attachShadow({ mode: "open" });
    shadowRoot.innerHTML = "<div id='shadowDiv'>E</div>";
  </script>
  `;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.content-visibility.enabled", true]],
  });
});

async function setContentVisibility(browser, value) {
  let mutationPromise = (() => {
    switch (value) {
      case "hidden":
        return waitForEvent(EVENT_REORDER, "target");
      case "auto":
        return waitForEvents({
          expected: [
            [EVENT_REORDER, "child"],
            [EVENT_REORDER, "content-child"],
            [EVENT_REORDER, "shadowDiv"],
            [EVENT_REORDER, "target"],
          ],
        });
      default:
        throw new Error(`unexpected content-visibility: ${value}`);
    }
  })();

  // Change the value of `content-visibility` property for the target
  info(`Setting content-visibility: ${value}`);
  await invokeSetAttribute(browser, "target", "class", value);
  await mutationPromise;
}

addAccessibleTask(
  snippet,
  async function (browser, accDoc) {
    const target = findAccessibleChildByID(accDoc, "target");

    info("Initial Accessibility Structure Test");
    testAccessibleTree(target, { SECTION: [] });

    await setContentVisibility(browser, "auto");
    testAccessibleTree(target, {
      SECTION: [
        { SECTION: [{ TEXT_LEAF: [] }] } /* child */,
        { SECTION: [{ TEXT_LEAF: [] }] } /* content-child */,
        { SECTION: [] } /* hidden-subtree-1 */,
        { SECTION: [{ SECTION: [{ TEXT_LEAF: [] }] }] } /* shadow-host */,
      ],
    });

    await setContentVisibility(browser, "hidden");
    testAccessibleTree(target, { SECTION: [] });
  },
  { iframe: true, remoteIframe: true, chrome: true }
);
