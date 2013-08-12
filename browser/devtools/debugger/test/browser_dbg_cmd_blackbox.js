/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the 'dbg blackbox' and 'dbg unblackbox' commands work as they
// should.

const TEST_URL = EXAMPLE_URL + "browser_dbg_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "blackboxing_blackboxme.js";
const BLACKBOXONE_URL = EXAMPLE_URL + "blackboxing_one.js";
const BLACKBOXTWO_URL = EXAMPLE_URL + "blackboxing_two.js";
const BLACKBOXTHREE_URL = EXAMPLE_URL + "blackboxing_three.js";

let gcli = Cu.import("resource://gre/modules/devtools/gcli.jsm", {}).gcli;

let gTarget;
let gPanel;
let gOptions;
let gDebugger;
let gClient;
let gThreadClient;
let gTab;

function cmd(typed, expectedNumEvents=1, output=null) {
  const deferred = promise.defer();

  let timesFired = 0;
  gThreadClient.addListener("blackboxchange", function _onBlackBoxChange() {
    if (++timesFired === expectedNumEvents) {
      gThreadClient.removeListener("blackboxchange", _onBlackBoxChange);
      deferred.resolve();
    }
  });

  let audit = {
    setup: typed,
    exec: {}
  };

  if (output) {
    audit.output = output;
  }

  helpers.audit(gOptions, [audit]);

  return deferred.promise;
}

function test() {
  helpers.addTabWithToolbar(TEST_URL, function(options) {
    gOptions = options;
    gTarget = options.target;
    return gDevTools.showToolbox(options.target, "jsdebugger")
      .then(setupGlobals)
      .then(waitForDebuggerSources)
      .then(testBlackBoxSource)
      .then(testUnBlackBoxSource)
      .then(testBlackBoxGlob)
      .then(testUnBlackBoxGlob)
      .then(testBlackBoxInvert)
      .then(testUnBlackBoxInvert)
      .then(null, function (error) {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      })
      .then(finishUp);
  });
}

function setupGlobals(toolbox) {
  gTab = gBrowser.selectedTab;
  gPanel = toolbox.getCurrentPanel();
  gDebugger = gPanel.panelWin;
  gClient = gDebugger.gClient;
  gThreadClient = gClient.activeThread;
}

function waitForDebuggerSources() {
  const deferred = promise.defer();
  gDebugger.addEventListener("Debugger:SourceShown", function _onSourceShown() {
    gDebugger.removeEventListener("Debugger:SourceShown", _onSourceShown, false);
    deferred.resolve();
  }, false);
  return deferred.promise;
}

function testBlackBoxSource() {
  return cmd("dbg blackbox " + BLACKBOXME_URL)
    .then(function () {
      const checkbox = getBlackBoxCheckbox(BLACKBOXME_URL);
      ok(!checkbox.checked,
         "Should be able to black box a specific source");
    });
}

function testUnBlackBoxSource() {
  return cmd("dbg unblackbox " + BLACKBOXME_URL)
    .then(function () {
      const checkbox = getBlackBoxCheckbox(BLACKBOXME_URL);
      ok(checkbox.checked,
         "Should be able to stop black boxing a specific source");
    });
}

function testBlackBoxGlob() {
  return cmd("dbg blackbox --glob *blackboxing_t*.js", 2,
             [/blackboxing_three\.js/g, /blackboxing_two\.js/g])
    .then(function () {
      ok(getBlackBoxCheckbox(BLACKBOXME_URL).checked,
         "blackboxme should not be black boxed because it doesn't match the glob");
      ok(getBlackBoxCheckbox(BLACKBOXONE_URL).checked,
         "blackbox_one should not be black boxed because it doesn't match the glob");

      ok(!getBlackBoxCheckbox(BLACKBOXTWO_URL).checked,
         "blackbox_two should be black boxed because it matches the glob");
      ok(!getBlackBoxCheckbox(BLACKBOXTHREE_URL).checked,
         "blackbox_three should be black boxed because it matches the glob");
    });
}

function testUnBlackBoxGlob() {
  return cmd("dbg unblackbox --glob *blackboxing_t*.js", 2)
    .then(function () {
      ok(getBlackBoxCheckbox(BLACKBOXTWO_URL).checked,
         "blackbox_two should be un-black boxed because it matches the glob");
      ok(getBlackBoxCheckbox(BLACKBOXTHREE_URL).checked,
         "blackbox_three should be un-black boxed because it matches the glob");
    });
}

function testBlackBoxInvert() {
  return cmd("dbg blackbox --invert --glob *blackboxing_t*.js", 3,
             [/blackboxing_three\.js/g, /blackboxing_two\.js/g])
    .then(function () {
      ok(!getBlackBoxCheckbox(BLACKBOXME_URL).checked,
         "blackboxme should be black boxed because it doesn't match the glob");
      ok(!getBlackBoxCheckbox(BLACKBOXONE_URL).checked,
         "blackbox_one should be black boxed because it doesn't match the glob");
      ok(!getBlackBoxCheckbox(TEST_URL).checked,
         "TEST_URL should be black boxed because it doesn't match the glob");

      ok(getBlackBoxCheckbox(BLACKBOXTWO_URL).checked,
         "blackbox_two should not be black boxed because it matches the glob");
      ok(getBlackBoxCheckbox(BLACKBOXTHREE_URL).checked,
         "blackbox_three should not be black boxed because it matches the glob");
    });
}

function testUnBlackBoxInvert() {
  return cmd("dbg unblackbox --invert --glob *blackboxing_t*.js", 3)
    .then(function () {
      ok(getBlackBoxCheckbox(BLACKBOXME_URL).checked,
         "blackboxme should be un-black boxed because it does not match the glob");
      ok(getBlackBoxCheckbox(BLACKBOXONE_URL).checked,
         "blackbox_one should be un-black boxed because it does not match the glob");
      ok(getBlackBoxCheckbox(TEST_URL).checked,
         "TEST_URL should be un-black boxed because it doesn't match the glob");
    });
}

function finishUp() {
  gTarget = null;
  gPanel = null;
  gOptions = null;
  gClient = null;
  gThreadClient = null;
  gDebugger = null;
  closeDebuggerAndFinish();
}

function getBlackBoxCheckbox(url) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item[tooltiptext=\""
      + url + "\"] .side-menu-widget-item-checkbox");
}
