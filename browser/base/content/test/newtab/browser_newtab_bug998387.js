/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  yield setLinks("0");
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
}
