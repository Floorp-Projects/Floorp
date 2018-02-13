/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that fonts are collapsed by default, and can be expanded.

const TEST_URI = URL_ROOT + "browser_fontinspector.html";

add_task(function* () {
  let { inspector, view } = yield openFontInspectorForURL(TEST_URI);
  let viewDoc = view.document;

  info("Checking all font are collapsed by default");
  let fonts = viewDoc.querySelectorAll("#all-fonts > li");
  checkAllFontsCollapsed(fonts);

  info("Clicking on the first one to expand the font details");
  yield expandFontDetails(fonts[0]);

  ok(fonts[0].querySelector(".theme-twisty").hasAttribute("open"), `Twisty is open`);
  ok(isFontDetailsVisible(fonts[0]), `Font details is shown`);

  info("Selecting a node with different fonts and checking that all fonts are collapsed");
  yield selectNode(".black-text", inspector);
  fonts = viewDoc.querySelectorAll("#all-fonts > li");
  checkAllFontsCollapsed(fonts);
});

function checkAllFontsCollapsed(fonts) {
  fonts.forEach((el, i) => {
    let twisty = el.querySelector(".theme-twisty");
    ok(twisty, `Twisty ${i} exists`);
    ok(!twisty.hasAttribute("open"), `Twisty ${i} is closed`);
    ok(!isFontDetailsVisible(el), `Font details ${i} is hidden`);
  });
}
