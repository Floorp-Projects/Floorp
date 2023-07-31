/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test a large update which adds many thousands of Accessibles with a
 * lot of content in each.
 */
addAccessibleTask(
  `<main id="main" hidden></main>`,
  async function (browser, docAcc) {
    let shown = waitForEvent(EVENT_SHOW, "main");
    await invokeContentTask(browser, [], () => {
      // Make a long string.
      let text = "";
      for (let i = 0; i < 100; ++i) {
        text += "a";
      }
      // Create lots of nodes which include the long string.
      const contMain = content.document.getElementById("main");
      // 15000 children of main.
      for (let w = 0; w < 15000; ++w) {
        // Each of those goes 9 deep.
        let parent = contMain;
        for (let d = 0; d < 10; ++d) {
          const div = content.document.createElement("div");
          div.setAttribute("aria-label", `${w} ${d} ${text}`);
          parent.append(div);
          parent = div;
        }
      }
      contMain.hidden = false;
    });
    const main = (await shown).accessible;
    is(main.childCount, 15000, "main has correct number of children");

    // We don't want to output passes for every check, since that would output
    // hundreds of thousands of lines, which slows the test to a crawl. Instead,
    // output any failures and keep track of overall success/failure.
    let treeOk = true;
    function check(val, msg) {
      if (!val) {
        ok(false, msg);
        treeOk = false;
      }
    }

    info("Checking tree");
    for (let w = 0; w < 15000; ++w) {
      let acc = main.getChildAt(w);
      let parent = main;
      for (let d = 0; d < 10; ++d) {
        check(acc, `Got child ${w} depth ${d}`);
        const name = `${w} ${d}`;
        check(acc.name.startsWith(name + " "), `${name}: correct name`);
        check(acc.parent == parent, `${name}: correct parent`);
        parent = acc;
        acc = acc.firstChild;
      }
    }
    // check() sets treeOk to false for any failure.
    ok(treeOk, "Tree is correct");
  }
);
