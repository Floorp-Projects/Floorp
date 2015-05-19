/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that enabling Service Workers testing option enables the
// mServiceWorkersTestingEnabled attribute added to nsPIDOMWindow.

const TEST_URI = URL_ROOT +
                 "browser_toolbox_options_enable_serviceworkers_testing.html";

const ELEMENT_ID = "devtools-enable-serviceWorkersTesting";

let toolbox;
let doc;

function test() {
  // Note: Pref dom.serviceWorkers.testing.enabled is false since we are testing
  // the same capabilities are enabled with the devtool pref.
  SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", false]
  ]}, start);
}

function start() {
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  content.location = TEST_URI;
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  toolbox.once("options-selected", () => {
    testRegisterFails().then(testRegisterInstallingWorker);
  });
  toolbox.selectTool("options");
}

function testRegisterFails() {
  let deferred = promise.defer();

  let output = doc.getElementById("output");
  let button = doc.getElementById("button");

  function doTheCheck() {
    info("Testing it doesn't registers correctly until enable testing");
    is(output.textContent,
       "SecurityError",
       "SecurityError expected");
    deferred.resolve();
  }

  if (output.textContent !== "No output") {
    doTheCheck();
  }

  button.addEventListener('click', function onClick() {
    button.removeEventListener('click', onClick);
    doTheCheck();
  });

  return deferred.promise;
}

function testRegisterInstallingWorker() {
  toggleServiceWorkersTestingCheckbox().then(() => {
    let output = doc.getElementById("output");
    let button = doc.getElementById("button");

    function doTheCheck() {
      info("Testing it registers correctly and there is an installing worker");
      is(output.textContent,
         "Installing worker/",
         "Installing worker expected");
      testRegisterFailsWhenToolboxCloses();
    }

    if (output.textContent !== "No output") {
      doTheCheck();
    }

    button.addEventListener('click', function onClick() {
      button.removeEventListener('click', onClick);
      doTheCheck();
    });
  });
}

// Workers should be turned back off when we closes the toolbox
function testRegisterFailsWhenToolboxCloses() {
  info("Testing it disable worker when closing the toolbox");
  toolbox.destroy()
         .then(reload)
         .then(testRegisterFails)
         .then(finishUp);
}

function reload() {
  let deferred = promise.defer();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    doc = content.document;
    deferred.resolve();
  }, true);

  let mm = getFrameScript();
  mm.sendAsyncMessage("devtools:test:reload");

  return deferred.promise;
}

function toggleServiceWorkersTestingCheckbox() {
  let deferred = promise.defer();

  let panel = toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById(ELEMENT_ID);

  cbx.scrollIntoView();

  if (cbx.checked) {
    info("Clearing checkbox to disable service workers testing");
  } else {
    info("Checking checkbox to enable service workers testing");
  }

  cbx.click();

  return reload();
}

function finishUp() {
  gBrowser.removeCurrentTab();
  toolbox = doc = null;
  finish();
}
