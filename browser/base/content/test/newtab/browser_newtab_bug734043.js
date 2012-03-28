/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  let receivedError = false;
  let block = getContentDocument().querySelector(".newtab-control-block");

  function onError() {
    receivedError = true;
  }

  let cw = getContentWindow();
  cw.addEventListener("error", onError);

  for (let i = 0; i < 3; i++)
    EventUtils.synthesizeMouseAtCenter(block, {}, cw);

  yield whenPagesUpdated();
  ok(!receivedError, "we got here without any errors");
  cw.removeEventListener("error", onError);
}
