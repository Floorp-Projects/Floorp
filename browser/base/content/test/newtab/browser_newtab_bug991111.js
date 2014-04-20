/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  yield setLinks("0");
  yield addNewTabPageTab();

  // Remember if the click handler was triggered
  let cell = getCell(0);
  let clicked = false;
  cell.site.onClick = e => {
    clicked = true;
    executeSoon(TestRunner.next);
  };

  // Send a middle-click and make sure it happened
  yield EventUtils.synthesizeMouseAtCenter(cell.node, {button: 1}, getContentWindow());
  ok(clicked, "middle click triggered click listener");
}
