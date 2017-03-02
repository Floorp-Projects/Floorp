/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Testing source search
add_task(function* () {
  const dbg = yield initDebugger("doc-script-switching.html");

  pressKey(dbg, "sourceSearch");
  yield waitForElement(dbg, "input");
  findElementWithSelector(dbg, "input").focus();
  type(dbg, "sw");
  pressKey(dbg, "Enter");

  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
  let source = dbg.selectors.getSelectedSource(dbg.getState());
  ok(source.get("url").match(/switching-01/), "first source is selected");

  // 2. arrow keys and check to see if source is selected
  pressKey(dbg, "sourceSearch");
  findElementWithSelector(dbg, "input").focus();
  type(dbg, "sw");
  pressKey(dbg, "Down");
  pressKey(dbg, "Enter");

  yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
  source = dbg.selectors.getSelectedSource(dbg.getState());
  ok(source.get("url").match(/switching-02/), "second source is selected");
});
