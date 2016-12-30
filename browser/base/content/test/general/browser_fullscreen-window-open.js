Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

const PREF_DISABLE_OPEN_NEW_WINDOW = "browser.link.open_newwindow.disabled_in_fullscreen";
const isOSX = (Services.appinfo.OS === "Darwin");

const TEST_FILE = "file_fullscreen-window-open.html";
const gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                          "http://127.0.0.1:8888/");

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, true);

  let newTab = gBrowser.addTab(gHttpTestRoot + TEST_FILE);
  gBrowser.selectedTab = newTab;

  whenTabLoaded(newTab, function() {
    // Enter browser fullscreen mode.
    BrowserFullScreen();

    runNextTest();
  });
}

registerCleanupFunction(function() {
  // Exit browser fullscreen mode.
  BrowserFullScreen();

  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(PREF_DISABLE_OPEN_NEW_WINDOW);
});

var gTests = [
  test_open,
  test_open_with_size,
  test_open_with_pos,
  test_open_with_outerSize,
  test_open_with_innerSize,
  test_open_with_dialog,
  test_open_when_open_new_window_by_pref,
  test_open_with_pref_to_disable_in_fullscreen,
  test_open_from_chrome,
];

function runNextTest() {
  let testCase = gTests.shift();
  if (testCase) {
    executeSoon(testCase);
  }
  else {
    finish();
  }
}


// Test for window.open() with no feature.
function test_open() {
  waitForTabOpen({
    message: {
      title: "test_open",
      param: "",
    },
    finalizeFn() {},
  });
}

// Test for window.open() with width/height.
function test_open_with_size() {
  waitForTabOpen({
    message: {
      title: "test_open_with_size",
      param: "width=400,height=400",
    },
    finalizeFn() {},
  });
}

// Test for window.open() with top/left.
function test_open_with_pos() {
  waitForTabOpen({
    message: {
      title: "test_open_with_pos",
      param: "top=200,left=200",
    },
    finalizeFn() {},
  });
}

// Test for window.open() with outerWidth/Height.
function test_open_with_outerSize() {
  let [outerWidth, outerHeight] = [window.outerWidth, window.outerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_outerSize",
      param: "outerWidth=200,outerHeight=200",
    },
    successFn() {
      is(window.outerWidth, outerWidth, "Don't change window.outerWidth.");
      is(window.outerHeight, outerHeight, "Don't change window.outerHeight.");
    },
    finalizeFn() {},
  });
}

// Test for window.open() with innerWidth/Height.
function test_open_with_innerSize() {
  let [innerWidth, innerHeight] = [window.innerWidth, window.innerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_innerSize",
      param: "innerWidth=200,innerHeight=200",
    },
    successFn() {
      is(window.innerWidth, innerWidth, "Don't change window.innerWidth.");
      is(window.innerHeight, innerHeight, "Don't change window.innerHeight.");
    },
    finalizeFn() {},
  });
}

// Test for window.open() with dialog.
function test_open_with_dialog() {
  waitForTabOpen({
    message: {
      title: "test_open_with_dialog",
      param: "dialog=yes",
    },
    finalizeFn() {},
  });
}

// Test for window.open()
// when "browser.link.open_newwindow" is nsIBrowserDOMWindow.OPEN_NEWWINDOW
function test_open_when_open_new_window_by_pref() {
  const PREF_NAME = "browser.link.open_newwindow";
  Services.prefs.setIntPref(PREF_NAME, Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW);
  is(Services.prefs.getIntPref(PREF_NAME), Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW,
     PREF_NAME + " is nsIBrowserDOMWindow.OPEN_NEWWINDOW at this time");

  waitForTabOpen({
    message: {
      title: "test_open_when_open_new_window_by_pref",
      param: "width=400,height=400",
    },
    finalizeFn() {
      Services.prefs.clearUserPref(PREF_NAME);
    },
  });
}

// Test for the pref, "browser.link.open_newwindow.disabled_in_fullscreen"
function test_open_with_pref_to_disable_in_fullscreen() {
  Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, false);

  waitForWindowOpen({
    message: {
      title: "test_open_with_pref_disabled_in_fullscreen",
      param: "width=400,height=400",
    },
    finalizeFn() {
      Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, true);
    },
  });
}


// Test for window.open() called from chrome context.
function test_open_from_chrome() {
  waitForWindowOpenFromChrome({
    message: {
      title: "test_open_from_chrome",
      param: "",
    },
    finalizeFn() {}
  });
}

function waitForTabOpen(aOptions) {
  let message = aOptions.message;

  if (!message.title) {
    ok(false, "Can't get message.title.");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onTabOpen = function onTabOpen(aEvent) {
    gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);

    let tab = aEvent.target;
    whenTabLoaded(tab, function() {
      is(tab.linkedBrowser.contentTitle, message.title,
         "Opened Tab is expected: " + message.title);

      if (aOptions.successFn) {
        aOptions.successFn();
      }

      gBrowser.removeTab(tab);
      finalize();
    });
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);

  let finalize = function() {
    aOptions.finalizeFn();
    info("Finished: " + message.title);
    runNextTest();
  };

  const URI = "data:text/html;charset=utf-8,<!DOCTYPE html><html><head><title>" +
              message.title +
              "<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";

  executeWindowOpenInContent({
    uri: URI,
    title: message.title,
    option: message.param,
  });
}


function waitForWindowOpen(aOptions) {
  let message = aOptions.message;
  let url = aOptions.url || "about:blank";

  if (!message.title) {
    ok(false, "Can't get message.title");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onFinalize = function() {
    aOptions.finalizeFn();

    info("Finished: " + message.title);
    runNextTest();
  };

  let listener = new WindowListener(message.title, getBrowserURL(), {
    onSuccess: aOptions.successFn,
    onFinalize,
  });
  Services.wm.addListener(listener);

  executeWindowOpenInContent({
    uri: url,
    title: message.title,
    option: message.param,
  });
}

function executeWindowOpenInContent(aParam) {
  ContentTask.spawn(gBrowser.selectedBrowser, JSON.stringify(aParam), function* (dataTestParam) {
    let testElm = content.document.getElementById("test");
    testElm.setAttribute("data-test-param", dataTestParam);
    testElm.click();
  });
}

function waitForWindowOpenFromChrome(aOptions) {
  let message = aOptions.message;
  let url = aOptions.url || "about:blank";

  if (!message.title) {
    ok(false, "Can't get message.title");
    aOptions.finalizeFn();
    runNextTest();
    return;
  }

  info("Running test: " + message.title);

  let onFinalize = function() {
    aOptions.finalizeFn();

    info("Finished: " + message.title);
    runNextTest();
  };

  let listener = new WindowListener(message.title, getBrowserURL(), {
    onSuccess: aOptions.successFn,
    onFinalize,
  });
  Services.wm.addListener(listener);

  window.open(url, message.title, message.option);
}

function WindowListener(aTitle, aUrl, aCallBackObj) {
  this.test_title = aTitle;
  this.test_url = aUrl;
  this.callback_onSuccess = aCallBackObj.onSuccess;
  this.callBack_onFinalize = aCallBackObj.onFinalize;
}
WindowListener.prototype = {

  test_title: null,
  test_url: null,
  callback_onSuccess: null,
  callBack_onFinalize: null,

  onOpenWindow(aXULWindow) {
    Services.wm.removeListener(this);

    let domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindow);
    let onLoad = aEvent => {
      is(domwindow.document.location.href, this.test_url,
        "Opened Window is expected: " + this.test_title);
      if (this.callback_onSuccess) {
        this.callback_onSuccess();
      }

      domwindow.removeEventListener("load", onLoad, true);

      // wait for trasition to fullscreen on OSX Lion later
      if (isOSX) {
        setTimeout(function() {
          domwindow.close();
          executeSoon(this.callBack_onFinalize);
        }.bind(this), 3000);
      }
      else {
        domwindow.close();
        executeSoon(this.callBack_onFinalize);
      }
    };
    domwindow.addEventListener("load", onLoad, true);
  },
  onCloseWindow(aXULWindow) {},
  onWindowTitleChange(aXULWindow, aNewTitle) {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediatorListener,
                                         Ci.nsISupports]),
};
