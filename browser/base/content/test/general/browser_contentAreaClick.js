/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 549340.
 * Test for browser.js::contentAreaClick() util.
 *
 * The test opens a new browser window, then replaces browser.js methods invoked
 * by contentAreaClick with a mock function that tracks which methods have been
 * called.
 * Each sub-test synthesizes a mouse click event on links injected in content,
 * the event is collected by a click handler that ensures that contentAreaClick
 * correctly prevent default events, and follows the correct code path.
 */

const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

var gTests = [

  {
    desc: "Simple left click",
    setup() {},
    clean() {},
    event: {},
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [],
    preventDefault: false,
  },

  {
    desc: "Ctrl/Cmd left click",
    setup() {},
    clean() {},
    event: { ctrlKey: true,
             metaKey: true },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  // The next test should just be like Alt click.
  {
    desc: "Shift+Alt left click",
    setup() {
      Services.prefs.setBoolPref("browser.altClickSave", true);
    },
    clean() {
      Services.prefs.clearUserPref("browser.altClickSave");
    },
    event: { shiftKey: true,
             altKey: true },
    targets: [ "commonlink", "maplink" ],
    expectedInvokedMethods: [ "gatherTextUnder", "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Shift+Alt left click on XLinks",
    setup() {
      Services.prefs.setBoolPref("browser.altClickSave", true);
    },
    clean() {
      Services.prefs.clearUserPref("browser.altClickSave");
    },
    event: { shiftKey: true,
             altKey: true },
    targets: [ "mathxlink", "svgxlink"],
    expectedInvokedMethods: [ "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Shift click",
    setup() {},
    clean() {},
    event: { shiftKey: true },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Alt click",
    setup() {
      Services.prefs.setBoolPref("browser.altClickSave", true);
    },
    clean() {
      Services.prefs.clearUserPref("browser.altClickSave");
    },
    event: { altKey: true },
    targets: [ "commonlink", "maplink" ],
    expectedInvokedMethods: [ "gatherTextUnder", "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Alt click on XLinks",
    setup() {
      Services.prefs.setBoolPref("browser.altClickSave", true);
    },
    clean() {
      Services.prefs.clearUserPref("browser.altClickSave");
    },
    event: { altKey: true },
    targets: [ "mathxlink", "svgxlink" ],
    expectedInvokedMethods: [ "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Panel click",
    setup() {},
    clean() {},
    event: {},
    targets: [ "panellink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "loadURI" ],
    preventDefault: true,
  },

  {
    desc: "Simple middle click opentab",
    setup() {},
    clean() {},
    event: { button: 1 },
    wantedEvent: "auxclick",
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Simple middle click openwin",
    setup() {
      Services.prefs.setBoolPref("browser.tabs.opentabfor.middleclick", false);
    },
    clean() {
      Services.prefs.clearUserPref("browser.tabs.opentabfor.middleclick");
    },
    event: { button: 1 },
    wantedEvent: "auxclick",
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Middle mouse paste",
    setup() {
      Services.prefs.setBoolPref("middlemouse.contentLoadURL", true);
      Services.prefs.setBoolPref("general.autoScroll", false);
    },
    clean() {
      Services.prefs.clearUserPref("middlemouse.contentLoadURL");
      Services.prefs.clearUserPref("general.autoScroll");
    },
    event: { button: 1 },
    wantedEvent: "auxclick",
    targets: [ "emptylink" ],
    expectedInvokedMethods: [ "middleMousePaste" ],
    preventDefault: true,
  },

];

// Array of method names that will be replaced in the new window.
var gReplacedMethods = [
  "middleMousePaste",
  "urlSecurityCheck",
  "loadURI",
  "gatherTextUnder",
  "saveURL",
  "openLinkIn",
  "getShortcutOrURIAndPostData",
];

// Returns the target object for the replaced method.
function getStub(replacedMethod) {
  let targetObj = replacedMethod == "getShortcutOrURIAndPostData" ? UrlbarUtils : gTestWin;
  return targetObj[replacedMethod];
}

// Reference to the new window.
var gTestWin = null;

// The test currently running.
var gCurrentTest = null;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    sinon.restore();
  });

  gTestWin = openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  whenDelayedStartupFinished(gTestWin, function() {
    info("Browser window opened");
    waitForFocus(function() {
      info("Browser window focused");
      waitForFocus(function() {
        info("Setting up browser...");
        setupTestBrowserWindow();
        info("Running tests...");
        executeSoon(runNextTest);
      }, gTestWin.content, true);
    }, gTestWin);
  });
}

// Click handler used to steal click events.
var gClickHandler = {
  handleEvent(event) {
    if (event.type == "click" && event.button != 0) {
      return;
    }
    let linkId = event.target.id || event.target.localName;
    let wantedEvent = gCurrentTest.wantedEvent || "click";
    is(event.type, wantedEvent,
       `${gCurrentTest.desc}:Handler received a ${wantedEvent} event on ${linkId}`);

    let isPanelClick = linkId == "panellink";
    gTestWin.contentAreaClick(event, isPanelClick);
    let prevent = event.defaultPrevented;
    is(prevent, gCurrentTest.preventDefault,
       gCurrentTest.desc + ": event.defaultPrevented is correct (" + prevent + ")");

    // Check that all required methods have been called.
    for (let expectedMethod of gCurrentTest.expectedInvokedMethods) {
      ok(getStub(expectedMethod).called,
        `${gCurrentTest.desc}:${expectedMethod} should have been invoked`);
    }

    for (let method of gReplacedMethods) {
      if (getStub(method).called &&
          !gCurrentTest.expectedInvokedMethods.includes(method)) {
        ok(false, `Should have not called ${method}`);
      }
    }

    event.preventDefault();
    event.stopPropagation();

    executeSoon(runNextTest);
  },
};

function setupTestBrowserWindow() {
  // Steal click events and don't propagate them.
  gTestWin.addEventListener("click", gClickHandler, true);
  gTestWin.addEventListener("auxclick", gClickHandler, true);

  // Replace methods.
  gReplacedMethods.forEach(function(methodName) {
    let targetObj = methodName == "getShortcutOrURIAndPostData" ? UrlbarUtils : gTestWin;
    sinon.stub(targetObj, methodName).returnsArg(0);
  });

  // Inject links in content.
  let doc = gTestWin.content.document;
  let mainDiv = doc.createElement("div");
  mainDiv.innerHTML =
    '<p><a id="commonlink" href="http://mochi.test/moz/">Common link</a></p>' +
    '<p><a id="panellink" href="http://mochi.test/moz/">Panel link</a></p>' +
    '<p><a id="emptylink">Empty link</a></p>' +
    '<p><math id="mathxlink" xmlns="http://www.w3.org/1998/Math/MathML" xlink:type="simple" xlink:href="http://mochi.test/moz/"><mtext>MathML XLink</mtext></math></p>' +
    '<p><svg id="svgxlink" xmlns="http://www.w3.org/2000/svg" width="100px" height="50px" version="1.1"><a xlink:type="simple" xlink:href="http://mochi.test/moz/"><text transform="translate(10, 25)">SVG XLink</text></a></svg></p>' +
    '<p><map name="map" id="map"><area href="http://mochi.test/moz/" shape="rect" coords="0,0,128,128" /></map><img id="maplink" usemap="#map" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAIAAABMXPacAAAABGdBTUEAALGPC%2FxhBQAAAOtJREFUeF7t0IEAAAAAgKD9qRcphAoDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGBgwIAAAT0N51AAAAAASUVORK5CYII%3D"/></p>';
  doc.body.appendChild(mainDiv);
}

function runNextTest() {
  if (!gCurrentTest) {
    gCurrentTest = gTests.shift();
    gCurrentTest.setup();
  }

  if (gCurrentTest.targets.length == 0) {
    info(gCurrentTest.desc + ": cleaning up...");
    gCurrentTest.clean();

    if (gTests.length > 0) {
      gCurrentTest = gTests.shift();
      gCurrentTest.setup();
    } else {
      finishTest();
      return;
    }
  }

  // Move to next target.
  sinon.resetHistory();
  let target = gCurrentTest.targets.shift();

  info(gCurrentTest.desc + ": testing " + target);

  // Fire (aux)click event.
  let targetElt = gTestWin.content.document.getElementById(target);
  ok(targetElt, gCurrentTest.desc + ": target is valid (" + targetElt.id + ")");
  EventUtils.synthesizeMouseAtCenter(targetElt, gCurrentTest.event, gTestWin.content);
}

function finishTest() {
  info("Restoring browser...");
  gTestWin.removeEventListener("click", gClickHandler, true);
  gTestWin.removeEventListener("auxclick", gClickHandler, true);
  gTestWin.close();
  finish();
}
