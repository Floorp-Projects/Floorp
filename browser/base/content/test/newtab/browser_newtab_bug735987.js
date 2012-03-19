/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(cells[1]);
  checkGrid("0,99p,1,2,3,4,5,6,7");

  yield blockCell(cells[1]);
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(cells[1]);
  checkGrid("0,99p,1,2,3,4,5,6,7");

  yield blockCell(cells[1]);
  checkGrid("0,1,2,3,4,5,6,7,8");
}
