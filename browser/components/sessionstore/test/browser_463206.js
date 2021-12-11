/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MOCHI_ROOT = ROOT.replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);
if (gFissionBrowser) {
  addCoopTask(
    "browser_463206_sample.html",
    test_restore_text_data_subframes,
    HTTPSROOT
  );
}
addNonCoopTask(
  "browser_463206_sample.html",
  test_restore_text_data_subframes,
  HTTPSROOT
);
addNonCoopTask(
  "browser_463206_sample.html",
  test_restore_text_data_subframes,
  HTTPROOT
);
addNonCoopTask(
  "browser_463206_sample.html",
  test_restore_text_data_subframes,
  MOCHI_ROOT
);

async function test_restore_text_data_subframes(aURL) {
  // Add a new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, aURL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // "Type in" some random values.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    function typeText(aTextField, aValue) {
      aTextField.value = aValue;

      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerGlobal, 0);
      aTextField.dispatchEvent(event);
    }
    typeText(content.document.getElementById("out1"), Date.now().toString(16));
    typeText(content.document.getElementsByName("1|#out2")[0], Math.random());
    await SpecialPowers.spawn(content.frames[0], [], async function() {
      await SpecialPowers.spawn(content.frames[1], [], async function() {
        function typeText2(aTextField, aValue) {
          aTextField.value = aValue;

          let event = aTextField.ownerDocument.createEvent("UIEvents");
          event.initUIEvent("input", true, true, aTextField.ownerGlobal, 0);
          aTextField.dispatchEvent(event);
        }
        typeText2(content.document.getElementById("in1"), new Date());
      });
    });
  });

  // Duplicate the tab.
  let tab2 = gBrowser.duplicateTab(tab);
  await promiseTabRestored(tab2);

  // Query a few values from the top and its child frames.
  await SpecialPowers.spawn(tab2.linkedBrowser, [], async function() {
    let out1Val = await SpecialPowers.spawn(
      content.frames[1],
      [],
      async function() {
        return content.document.getElementById("out1").value;
      }
    );
    Assert.notEqual(
      content.document.getElementById("out1").value,
      out1Val,
      "text isn't reused for frames"
    );
    Assert.notEqual(
      content.document.getElementsByName("1|#out2")[0].value,
      "",
      "text containing | and # is correctly restored"
    );
    let out2Val = await SpecialPowers.spawn(
      content.frames[1],
      [],
      async function() {
        return content.document.getElementById("out2").value;
      }
    );
    Assert.equal(out2Val, "", "id prefixes can't be faked");
    let in1ValFrame0_1 = await SpecialPowers.spawn(
      content.frames[0],
      [],
      async function() {
        return SpecialPowers.spawn(content.frames[1], [], async function() {
          return content.document.getElementById("in1").value;
        });
      }
    );
    // Bug 588077
    todo_is(in1ValFrame0_1, "", "id prefixes aren't mixed up");

    let in1ValFrame1_0 = await SpecialPowers.spawn(
      content.frames[1],
      [],
      async function() {
        return SpecialPowers.spawn(content.frames[0], [], async function() {
          return content.document.getElementById("in1").value;
        });
      }
    );
    Assert.equal(in1ValFrame1_0, "", "id prefixes aren't mixed up");
  });

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
}
