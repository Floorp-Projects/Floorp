/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NEWTAB_ROWS = "browser.newtabpage.rows";

function runTests() {
  // set max rows to 1, to avoid scroll events by clicking middle button
  Services.prefs.setIntPref(PREF_NEWTAB_ROWS, 1);
  yield setLinks("0");
  yield addNewTabPageTab();
  // we need a second newtab to honor max rows
  yield addNewTabPageTab();

  // Remember if the click handler was triggered
  let {site} = getCell(0);
  let origOnClick = site.onClick;
  let clicked = false;
  site.onClick = e => {
    origOnClick.call(site, e);
    clicked = true;
    executeSoon(TestRunner.next);
  };

  // Send a middle-click and make sure it happened
  let block = getContentDocument().querySelector(".newtab-control-block");
  yield EventUtils.synthesizeMouseAtCenter(block, {button: 1}, getContentWindow());
  ok(clicked, "middle click triggered click listener");

  // Make sure the cell didn't actually get blocked
  checkGrid("0");
  Services.prefs.clearUserPref(PREF_NEWTAB_ROWS);
}
