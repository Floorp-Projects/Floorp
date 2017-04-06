"use strict";

/**
 * Ensure redundant style flushes are not triggered when switching between windows
 */
add_task(function* test_toolbar_element_restyles_on_activation() {
  let restyles = {
    win1:  {},
    win2:  {}
  };

  // create a window and snapshot the elementsStyled
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, win1));

  // create a 2nd window and snapshot the elementsStyled
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, win2));

  let utils1 = SpecialPowers.getDOMWindowUtils(win1);
  restyles.win1.initial = utils1.elementsRestyled;

  let utils2 = SpecialPowers.getDOMWindowUtils(win2);
  restyles.win2.initial = utils2.elementsRestyled;

  // switch back to 1st window, and snapshot elementsStyled
  win1.focus();
  restyles.win1.activate = utils1.elementsRestyled;
  restyles.win2.deactivate = utils2.elementsRestyled;

  // switch back to 2nd window, and snapshot elementsStyled
  win2.focus();
  restyles.win2.activate = utils2.elementsRestyled;
  restyles.win1.deactivate = utils1.elementsRestyled;

  is(restyles.win1.activate - restyles.win1.deactivate, 0,
      "No elements restyled when re-activating/deactivating a window");
  is(restyles.win2.activate - restyles.win2.deactivate, 0,
      "No elements restyled when re-activating/deactivating a window");

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});
