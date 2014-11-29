/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Protocol error (unknownError): Error: Got an invalid root window in DocumentWalker");

const TEST_URI = "data:text/html;charset=utf-8,<p>browser_telemetry_button_scratchpad.js</p>";

// Because we need to gather stats for the period of time that a tool has been
// opened we make use of setTimeout() to create tool active times.
const TOOL_DELAY = 200;

let promise = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {}).Promise;

let numScratchpads = 0;

function init() {
  startTelemetry();

  Services.ww.registerNotification(windowObserver);
  testButton("command-button-scratchpad");
}

function testButton(id) {
  info("Testing " + id);

  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    info("inspector opened");

    let button = toolbox.doc.querySelector("#" + id);
    ok(button, "Captain, we have the button");

    delayedClicks(button, 4).then(null, console.error);
  }).then(null, console.error);
}

function windowObserver(aSubject, aTopic, aData) {
  if (aTopic == "domwindowopened") {
    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);

      if (win.Scratchpad) {
        win.Scratchpad.addObserver({
          onReady: function() {
            win.Scratchpad.removeObserver(this);
            numScratchpads++;
            win.close();

            info("another scratchpad was opened and closed, count is now " + numScratchpads);

            if (numScratchpads === 4) {
              Services.ww.unregisterNotification(windowObserver);
              info("4 scratchpads have been opened and closed, checking results");
              checkResults();
            }
          },
        });
      }
    }, false);
  }
}

function delayedClicks(node, clicks) {
  let deferred = promise.defer();
  let clicked = 0;

  // See TOOL_DELAY for why we need setTimeout here
  setTimeout(function delayedClick() {
    info("Clicking button " + node.id);
    node.click();
    clicked++;

    if (clicked >= clicks) {
      deferred.resolve(node);
    } else {
      setTimeout(delayedClick, TOOL_DELAY);
    }
  }, TOOL_DELAY);

  return deferred.promise;
}

function checkResults() {
  // For help generating these tests use generateTelemetryTests("DEVTOOLS_")
  // here.
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_LISTTABS_MS", null, "hasentries");
  checkTelemetry("DEVTOOLS_DEBUGGER_RDP_LOCAL_RECONFIGURETAB_MS", null, "hasentries");

  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_INSPECTOR_OPENED_PER_USER_FLAG", [0,1,0]);

  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_RULEVIEW_OPENED_PER_USER_FLAG", [0,1,0]);

  checkTelemetry("DEVTOOLS_SCRATCHPAD_OPENED_BOOLEAN", [0,4,0]);
  checkTelemetry("DEVTOOLS_SCRATCHPAD_OPENED_PER_USER_FLAG", [0,1,0]);

  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_BOOLEAN", [0,1,0]);
  checkTelemetry("DEVTOOLS_TOOLBOX_OPENED_PER_USER_FLAG", [0,1,0]);

  finishUp();
}

function finishUp() {
  gBrowser.removeCurrentTab();

  TargetFactory = promise = numScratchpads = null;

  finish();
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(init, content);
  }, true);

  content.location = TEST_URI;
}
