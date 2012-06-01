/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the edit command works

const TEST_URI = TEST_BASE_HTTP + "resources.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    testEditStatus(browser, tab);
    // Bug 759853
    // testEditExec(browser, tab); // calls finish()
    finish();
  });
}

function testEditStatus(browser, tab) {
  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit",
    markup: "VVVV",
    status: "ERROR",
    emptyParameters: [ " <resource>", " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit i",
    markup: "VVVVVI",
    status: "ERROR",
    directTabText: "nline-css",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit c",
    markup: "VVVVVI",
    status: "ERROR",
    directTabText: "ss#style2",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit http",
    markup: "VVVVVIIII",
    status: "ERROR",
    directTabText: "://example.com/browser/browser/devtools/commandline/test/resources_inpage1.css",
    arrowTabText: "",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit page1",
    markup: "VVVVVIIIII",
    status: "ERROR",
    directTabText: "",
    arrowTabText: "http://example.com/browser/browser/devtools/commandline/test/resources_inpage1.css",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit page2",
    markup: "VVVVVIIIII",
    status: "ERROR",
    directTabText: "",
    arrowTabText: "http://example.com/browser/browser/devtools/commandline/test/resources_inpage2.css",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit stylez",
    markup: "VVVVVEEEEEE",
    status: "ERROR",
    directTabText: "",
    arrowTabText: "",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit css#style2",
    markup: "VVVVVVVVVVVVVVV",
    status: "VALID",
    directTabText: "",
    emptyParameters: [ " [line]" ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit css#style2 5",
    markup: "VVVVVVVVVVVVVVVVV",
    status: "VALID",
    directTabText: "",
    emptyParameters: [ ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit css#style2 0",
    markup: "VVVVVVVVVVVVVVVVE",
    status: "ERROR",
    directTabText: "",
    emptyParameters: [ ],
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "edit css#style2 -1",
    markup: "VVVVVVVVVVVVVVVVEE",
    status: "ERROR",
    directTabText: "",
    emptyParameters: [ ],
  });
}

var windowListener = {
  onOpenWindow: function(win) {
    // Wait for the window to finish loading
    let win = win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowInternal || Ci.nsIDOMWindow);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      win.close();
    }, false);
    win.addEventListener("unload", function onUnload() {
      win.removeEventListener("unload", onUnload, false);
      Services.wm.removeListener(windowListener);
      finish();
    }, false);
  },
  onCloseWindow: function(win) { },
  onWindowTitleChange: function(win, title) { }
};

function testEditExec(browser, tab) {

  Services.wm.addListener(windowListener);

  var style2 = browser.contentDocument.getElementById("style2");
  DeveloperToolbarTest.exec({
    typed: "edit css#style2",
    args: {
      resource: function(resource) {
        return resource.element.ownerNode == style2;
      },
      line: 1
    },
    completed: true,
    blankOutput: true,
  });
}
