/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  yield setLinks("0");
  yield addNewTabPageTab();

  // When gSearch modifies the DOM as it sets itself up, it can prevent the
  // popup from opening, depending on the timing.  Wait until that's done.
  yield whenSearchInitDone();

  let site = getCell(0).node.querySelector(".newtab-site");
  site.setAttribute("type", "sponsored");

  let sponsoredPanel = getContentDocument().getElementById("sponsored-panel");
  is(sponsoredPanel.state, "closed", "Sponsored panel must be closed");

  function continueOnceOn(event) {
    sponsoredPanel.addEventListener(event, function listener() {
      sponsoredPanel.removeEventListener(event, listener);
      executeSoon(TestRunner.next);
    });
  }

  // test sponsoredPanel appearing upon a click
  continueOnceOn("popupshown");
  let sponsoredButton = site.querySelector(".newtab-control-sponsored");
  yield EventUtils.synthesizeMouseAtCenter(sponsoredButton, {}, getContentWindow());
  is(sponsoredPanel.state, "open", "Sponsored panel opens on click");
  ok(sponsoredButton.hasAttribute("panelShown"), "Sponsored button has panelShown attribute");

  // test sponsoredPanel hiding
  continueOnceOn("popuphidden");
  yield sponsoredPanel.hidePopup();
  is(sponsoredPanel.state, "closed", "Sponsored panel correctly closed/hidden");
  ok(!sponsoredButton.hasAttribute("panelShown"), "Sponsored button does not have panelShown attribute");
}
