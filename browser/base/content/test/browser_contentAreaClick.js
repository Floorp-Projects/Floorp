/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Browser Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
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

let gTests = [

  {
    desc: "Simple left click",
    setup: function() {},
    clean: function() {},
    event: {},
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [],
    preventDefault: false,
  },

  {
    desc: "Ctrl/Cmd left click",
    setup: function() {},
    clean: function() {},
    event: { ctrlKey: true,
             metaKey: true },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  // The next test was once handling feedService.forcePreview().  Now it should
  // just be like Alt click.
  {
    desc: "Shift+Alt left click",
    setup: function() {},
    clean: function() {},
    event: { shiftKey: true,
             altKey: true },
    targets: [ "commonlink", "maplink" ],
    expectedInvokedMethods: [ "gatherTextUnder", "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Shift+Alt left click on XLinks",
    setup: function() {},
    clean: function() {},
    event: { shiftKey: true,
             altKey: true },
    targets: [ "mathxlink", "svgxlink"],
    expectedInvokedMethods: [ "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Shift click",
    setup: function() {},
    clean: function() {},
    event: { shiftKey: true },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Alt click",
    setup: function() {},
    clean: function() {},
    event: { altKey: true },
    targets: [ "commonlink", "maplink" ],
    expectedInvokedMethods: [ "gatherTextUnder", "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Alt click on XLinks",
    setup: function() {},
    clean: function() {},
    event: { altKey: true },
    targets: [ "mathxlink", "svgxlink" ],
    expectedInvokedMethods: [ "saveURL" ],
    preventDefault: true,
  },

  {
    desc: "Panel click",
    setup: function() {},
    clean: function() {},
    event: {},
    targets: [ "panellink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "getShortcutOrURI", "loadURI" ],
    preventDefault: true,
  },

  {
    desc: "Simple middle click opentab",
    setup: function() {},
    clean: function() {},
    event: { button: 1 },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Simple middle click openwin",
    setup: function() {
      gPrefService.setBoolPref("browser.tabs.opentabfor.middleclick", false);
    },
    clean: function() {
      try {
        gPrefService.clearUserPref("browser.tabs.opentabfor.middleclick");
      } catch(ex) {}
    },
    event: { button: 1 },
    targets: [ "commonlink", "mathxlink", "svgxlink", "maplink" ],
    expectedInvokedMethods: [ "urlSecurityCheck", "openLinkIn" ],
    preventDefault: true,
  },

  {
    desc: "Middle mouse paste",
    setup: function() {
      gPrefService.setBoolPref("middlemouse.contentLoadURL", true);
      gPrefService.setBoolPref("general.autoScroll", false);
    },
    clean: function() {
      try {
        gPrefService.clearUserPref("middlemouse.contentLoadURL");
      } catch(ex) {}
      try {
        gPrefService.clearUserPref("general.autoScroll");
      } catch(ex) {}
    },
    event: { button: 1 },
    targets: [ "emptylink" ],
    expectedInvokedMethods: [ "middleMousePaste" ],
    preventDefault: true,
  },

];

// Array of method names that will be replaced in the new window.
let gReplacedMethods = [
  "middleMousePaste",
  "urlSecurityCheck",
  "loadURI",
  "gatherTextUnder",
  "saveURL",
  "openLinkIn",
  "getShortcutOrURI",
];

// Reference to the new window.
let gTestWin = null;

// List of methods invoked by a specific call to contentAreaClick.
let gInvokedMethods = [];

// The test currently running.
let gCurrentTest = null;

function test() {
  waitForExplicitFinish();

  gTestWin = openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    if (aSubject != gTestWin)
      return;

    Services.obs.removeObserver(arguments.callee, "browser-delayed-startup-finished");

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
  }, "browser-delayed-startup-finished", false);
}

// Click handler used to steal click events.
let gClickHandler = {
  handleEvent: function (event) {
    let linkId = event.target.id || event.target.localName;
    is(event.type, "click",
       gCurrentTest.desc + ":Handler received a click event on " + linkId);

    let isPanelClick = linkId == "panellink";
    gTestWin.contentAreaClick(event, isPanelClick);
    let prevent = event.defaultPrevented;
    is(prevent, gCurrentTest.preventDefault,
       gCurrentTest.desc + ": event.defaultPrevented is correct (" + prevent + ")")

    // Check that all required methods have been called.
    gCurrentTest.expectedInvokedMethods.forEach(function(aExpectedMethodName) {
      isnot(gInvokedMethods.indexOf(aExpectedMethodName), -1,
            gCurrentTest.desc + ":" + aExpectedMethodName + " was invoked");
    });
    
    if (gInvokedMethods.length != gCurrentTest.expectedInvokedMethods.length) {
      ok(false, "Wrong number of invoked methods");
      gInvokedMethods.forEach(function (method) info(method + " was invoked"));
    }

    event.preventDefault();
    event.stopPropagation();

    executeSoon(runNextTest);
  }
}

// Wraps around the methods' replacement mock function.
function wrapperMethod(aInvokedMethods, aMethodName) {
  return function () {
    aInvokedMethods.push(aMethodName);
    // At least getShortcutOrURI requires to return url that is the first param.
    return arguments[0];
  }
}

function setupTestBrowserWindow() {
  // Steal click events and don't propagate them.
  gTestWin.addEventListener("click", gClickHandler, true);

  // Replace methods.
  gReplacedMethods.forEach(function (aMethodName) {
    gTestWin["old_" + aMethodName] = gTestWin[aMethodName];
    gTestWin[aMethodName] = wrapperMethod(gInvokedMethods, aMethodName);
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
    '<p><map name="map" id="map"><area href="http://mochi.test/moz/" shape="rect" coords="0,0,128,128" /></map><img id="maplink" usemap="#map" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAIAAABMXPacAAAABGdBTUEAALGPC%2FxhBQAAAOtJREFUeF7t0IEAAAAAgKD9qRcphAoDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGDAgAEDBgwYMGBgwIAAAT0N51AAAAAASUVORK5CYII%3D"/></p>'
  doc.body.appendChild(mainDiv);
}

function runNextTest() {
  if (!gCurrentTest) {
    gCurrentTest = gTests.shift();
    gCurrentTest.setup();
  }

  if (gCurrentTest.targets.length == 0) {
    info(gCurrentTest.desc + ": cleaning up...")
    gCurrentTest.clean();

    if (gTests.length > 0) {
      gCurrentTest = gTests.shift();
      gCurrentTest.setup();
    }
    else {
      finishTest();
      return;
    }
  }

  // Move to next target.
  gInvokedMethods.length = 0;
  let target = gCurrentTest.targets.shift();

  info(gCurrentTest.desc + ": testing " + target);

  // Fire click event.
  let targetElt = gTestWin.content.document.getElementById(target);
  ok(targetElt, gCurrentTest.desc + ": target is valid (" + targetElt.id + ")");
  EventUtils.synthesizeMouseAtCenter(targetElt, gCurrentTest.event, gTestWin.content);
}

function finishTest() {
  info("Restoring browser...");
  gTestWin.removeEventListener("click", gClickHandler, true);

  // Restore original methods.
  gReplacedMethods.forEach(function (aMethodName) {
    gTestWin[aMethodName] = gTestWin["old_" + aMethodName];
    delete gTestWin["old_" + aMethodName];
  });

  gTestWin.close();
  finish();
}
