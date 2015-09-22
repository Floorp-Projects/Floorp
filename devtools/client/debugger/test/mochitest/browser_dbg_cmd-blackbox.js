/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the 'dbg blackbox' and 'dbg unblackbox' commands work as
 * they should.
 */

const TEST_URL = EXAMPLE_URL + "doc_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "code_blackboxing_blackboxme.js";
const BLACKBOXONE_URL = EXAMPLE_URL + "code_blackboxing_one.js";
const BLACKBOXTWO_URL = EXAMPLE_URL + "code_blackboxing_two.js";
const BLACKBOXTHREE_URL = EXAMPLE_URL + "code_blackboxing_three.js";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URL);
  yield helpers.openToolbar(options);

  let toolbox = yield gDevTools.showToolbox(options.target, "jsdebugger");
  let panel = toolbox.getCurrentPanel();

  yield waitForDebuggerEvents(panel, panel.panelWin.EVENTS.SOURCE_SHOWN);

  function cmd(aTyped, aEventRepeat = 1, aOutput = "") {
    return promise.all([
      waitForThreadEvents(panel, "blackboxchange", aEventRepeat),
      helpers.audit(options, [{ setup: aTyped, output: aOutput, exec: {} }])
    ]);
  }

  // test Black-Box Source
  yield cmd("dbg blackbox " + BLACKBOXME_URL);

  let bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXME_URL);
  ok(bbButton.checked,
     "Should be able to black box a specific source.");

  // test Un-Black-Box Source
  yield cmd("dbg unblackbox " + BLACKBOXME_URL);

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXME_URL);
  ok(!bbButton.checked,
     "Should be able to stop black boxing a specific source.");

  // test Black-Box Glob
  yield cmd("dbg blackbox --glob *blackboxing_t*.js", 2,
            [/blackboxing_three\.js/g, /blackboxing_two\.js/g]);

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXME_URL);
  ok(!bbButton.checked,
     "blackboxme should not be black boxed because it doesn't match the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXONE_URL);
  ok(!bbButton.checked,
     "blackbox_one should not be black boxed because it doesn't match the glob.");

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTWO_URL);
  ok(bbButton.checked,
     "blackbox_two should be black boxed because it matches the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTHREE_URL);
  ok(bbButton.checked,
    "blackbox_three should be black boxed because it matches the glob.");

  // test Un-Black-Box Glob
  yield cmd("dbg unblackbox --glob *blackboxing_t*.js", 2);

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTWO_URL);
  ok(!bbButton.checked,
     "blackbox_two should be un-black boxed because it matches the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTHREE_URL);
  ok(!bbButton.checked,
    "blackbox_three should be un-black boxed because it matches the glob.");

  // test Black-Box Invert
  yield cmd("dbg blackbox --invert --glob *blackboxing_t*.js", 3,
            [/blackboxing_three\.js/g, /blackboxing_two\.js/g]);

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXME_URL);
  ok(bbButton.checked,
    "blackboxme should be black boxed because it doesn't match the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXONE_URL);
  ok(bbButton.checked,
    "blackbox_one should be black boxed because it doesn't match the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, TEST_URL);
  ok(bbButton.checked,
    "TEST_URL should be black boxed because it doesn't match the glob.");

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTWO_URL);
  ok(!bbButton.checked,
    "blackbox_two should not be black boxed because it matches the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXTHREE_URL);
  ok(!bbButton.checked,
    "blackbox_three should not be black boxed because it matches the glob.");

  // test Un-Black-Box Invert
  yield cmd("dbg unblackbox --invert --glob *blackboxing_t*.js", 3);

  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXME_URL);
  ok(!bbButton.checked,
    "blackboxme should be un-black boxed because it does not match the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, BLACKBOXONE_URL);
  ok(!bbButton.checked,
    "blackbox_one should be un-black boxed because it does not match the glob.");
  bbButton = yield selectSourceAndGetBlackBoxButton(panel, TEST_URL);
  ok(!bbButton.checked,
    "TEST_URL should be un-black boxed because it doesn't match the glob.");

  yield teardown(panel, { noTabRemoval: true });
  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
