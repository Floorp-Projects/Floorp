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

let gPanel, gDebugger, gThreadClient;
let gOptions;

function test() {
  helpers.addTabWithToolbar(TEST_URL, function(aOptions) {
    gOptions = aOptions;

    return gDevTools.showToolbox(aOptions.target, "jsdebugger")
      .then(setupGlobals)
      .then(waitForDebuggerSources)
      .then(testBlackBoxSource)
      .then(testUnBlackBoxSource)
      .then(testBlackBoxGlob)
      .then(testUnBlackBoxGlob)
      .then(testBlackBoxInvert)
      .then(testUnBlackBoxInvert)
      .then(() => closeDebuggerAndFinish(gPanel, { noTabRemoval: true }))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function setupGlobals(aToolbox) {
  gPanel = aToolbox.getCurrentPanel();
  gDebugger = gPanel.panelWin;
  gThreadClient = gDebugger.gThreadClient;
}

function waitForDebuggerSources() {
  return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
}

function testBlackBoxSource() {
  return Task.spawn(function* () {
    yield cmd("dbg blackbox " + BLACKBOXME_URL);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXME_URL);
    ok(bbButton.checked,
       "Should be able to black box a specific source.");
  });
}

function testUnBlackBoxSource() {
  return Task.spawn(function* () {
    yield cmd("dbg unblackbox " + BLACKBOXME_URL);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXME_URL);
    ok(!bbButton.checked,
       "Should be able to stop black boxing a specific source.");
  });
}

function testBlackBoxGlob() {
  return Task.spawn(function* () {
    yield cmd("dbg blackbox --glob *blackboxing_t*.js", 2,
              [/blackboxing_three\.js/g, /blackboxing_two\.js/g]);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXME_URL);
    ok(!bbButton.checked,
       "blackboxme should not be black boxed because it doesn't match the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXONE_URL);
    ok(!bbButton.checked,
       "blackbox_one should not be black boxed because it doesn't match the glob.");

    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTWO_URL);
    ok(bbButton.checked,
       "blackbox_two should be black boxed because it matches the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTHREE_URL);
    ok(bbButton.checked,
      "blackbox_three should be black boxed because it matches the glob.");
  });
}

function testUnBlackBoxGlob() {
  return Task.spawn(function* () {
    yield cmd("dbg unblackbox --glob *blackboxing_t*.js", 2);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTWO_URL);
    ok(!bbButton.checked,
       "blackbox_two should be un-black boxed because it matches the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTHREE_URL);
    ok(!bbButton.checked,
      "blackbox_three should be un-black boxed because it matches the glob.");
  });
}

function testBlackBoxInvert() {
  return Task.spawn(function* () {
    yield cmd("dbg blackbox --invert --glob *blackboxing_t*.js", 3,
              [/blackboxing_three\.js/g, /blackboxing_two\.js/g]);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXME_URL);
    ok(bbButton.checked,
      "blackboxme should be black boxed because it doesn't match the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXONE_URL);
    ok(bbButton.checked,
      "blackbox_one should be black boxed because it doesn't match the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, TEST_URL);
    ok(bbButton.checked,
      "TEST_URL should be black boxed because it doesn't match the glob.");

    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTWO_URL);
    ok(!bbButton.checked,
      "blackbox_two should not be black boxed because it matches the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXTHREE_URL);
    ok(!bbButton.checked,
      "blackbox_three should not be black boxed because it matches the glob.");
  });
}

function testUnBlackBoxInvert() {
  return Task.spawn(function* () {
    yield cmd("dbg unblackbox --invert --glob *blackboxing_t*.js", 3);

    let bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXME_URL);
    ok(!bbButton.checked,
      "blackboxme should be un-black boxed because it does not match the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, BLACKBOXONE_URL);
    ok(!bbButton.checked,
      "blackbox_one should be un-black boxed because it does not match the glob.");
    bbButton = yield selectSourceAndGetBlackBoxButton(gPanel, TEST_URL);
    ok(!bbButton.checked,
      "TEST_URL should be un-black boxed because it doesn't match the glob.");
  });
}

registerCleanupFunction(function() {
  gOptions = null;
  gPanel = null;
  gDebugger = null;
  gThreadClient = null;
});

function cmd(aTyped, aEventRepeat = 1, aOutput = "") {
  return promise.all([
    waitForThreadEvents(gPanel, "blackboxchange", aEventRepeat),
    helpers.audit(gOptions, [{ setup: aTyped, output: aOutput, exec: {} }])
  ]);
}
