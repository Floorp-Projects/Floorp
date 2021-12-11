/* eslint-disable mozilla/no-arbitrary-setTimeout */

const PREF_DISABLE_OPEN_NEW_WINDOW =
  "browser.link.open_newwindow.disabled_in_fullscreen";
const PREF_BLOCK_TOPLEVEL_DATA =
  "security.data_uri.block_toplevel_data_uri_navigations";
const isOSX = Services.appinfo.OS === "Darwin";

const TEST_FILE = "file_fullscreen-window-open.html";
const gHttpTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

var newWin;
var newBrowser;

async function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_DISABLE_OPEN_NEW_WINDOW, true);
  Services.prefs.setBoolPref(PREF_BLOCK_TOPLEVEL_DATA, false);

  newWin = await BrowserTestUtils.openNewBrowserWindow();
  newBrowser = newWin.gBrowser;
  await promiseTabLoadEvent(newBrowser.selectedTab, gHttpTestRoot + TEST_FILE);

  // Enter browser fullscreen mode.
  newWin.BrowserFullScreen();

  runNextTest();
}

registerCleanupFunction(async function() {
  // Exit browser fullscreen mode.
  newWin.BrowserFullScreen();

  await BrowserTestUtils.closeWindow(newWin);

  Services.prefs.clearUserPref(PREF_DISABLE_OPEN_NEW_WINDOW);
  Services.prefs.clearUserPref(PREF_BLOCK_TOPLEVEL_DATA);
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
  } else {
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
  let [outerWidth, outerHeight] = [newWin.outerWidth, newWin.outerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_outerSize",
      param: "outerWidth=200,outerHeight=200",
    },
    successFn() {
      is(newWin.outerWidth, outerWidth, "Don't change window.outerWidth.");
      is(newWin.outerHeight, outerHeight, "Don't change window.outerHeight.");
    },
    finalizeFn() {},
  });
}

// Test for window.open() with innerWidth/Height.
function test_open_with_innerSize() {
  let [innerWidth, innerHeight] = [newWin.innerWidth, newWin.innerHeight];
  waitForTabOpen({
    message: {
      title: "test_open_with_innerSize",
      param: "innerWidth=200,innerHeight=200",
    },
    successFn() {
      is(newWin.innerWidth, innerWidth, "Don't change window.innerWidth.");
      is(newWin.innerHeight, innerHeight, "Don't change window.innerHeight.");
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
  is(
    Services.prefs.getIntPref(PREF_NAME),
    Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW,
    PREF_NAME + " is nsIBrowserDOMWindow.OPEN_NEWWINDOW at this time"
  );

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
      option: "noopener",
    },
    finalizeFn() {},
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
    newBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);

    let tab = aEvent.target;
    whenTabLoaded(tab, function() {
      is(
        tab.linkedBrowser.contentTitle,
        message.title,
        "Opened Tab is expected: " + message.title
      );

      if (aOptions.successFn) {
        aOptions.successFn();
      }

      newBrowser.removeTab(tab);
      finalize();
    });
  };
  newBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);

  let finalize = function() {
    aOptions.finalizeFn();
    info("Finished: " + message.title);
    runNextTest();
  };

  const URI =
    "data:text/html;charset=utf-8,<!DOCTYPE html><html><head><title>" +
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

  let listener = new WindowListener(
    message.title,
    AppConstants.BROWSER_CHROME_URL,
    {
      onSuccess: aOptions.successFn,
      onFinalize,
    }
  );
  Services.wm.addListener(listener);

  executeWindowOpenInContent({
    uri: url,
    title: message.title,
    option: message.param,
  });
}

function executeWindowOpenInContent(aParam) {
  SpecialPowers.spawn(
    newBrowser.selectedBrowser,
    [JSON.stringify(aParam)],
    async function(dataTestParam) {
      let testElm = content.document.getElementById("test");
      testElm.setAttribute("data-test-param", dataTestParam);
      testElm.click();
    }
  );
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

  let listener = new WindowListener(
    message.title,
    AppConstants.BROWSER_CHROME_URL,
    {
      onSuccess: aOptions.successFn,
      onFinalize,
    }
  );
  Services.wm.addListener(listener);

  newWin.open(url, message.title, message.option);
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

    let domwindow = aXULWindow.docShell.domWindow;
    let onLoad = aEvent => {
      is(
        domwindow.document.location.href,
        this.test_url,
        "Opened Window is expected: " + this.test_title
      );
      if (this.callback_onSuccess) {
        this.callback_onSuccess();
      }

      domwindow.removeEventListener("load", onLoad, true);

      // wait for trasition to fullscreen on OSX Lion later
      if (isOSX) {
        setTimeout(() => {
          domwindow.close();
          executeSoon(this.callBack_onFinalize);
        }, 3000);
      } else {
        domwindow.close();
        executeSoon(this.callBack_onFinalize);
      }
    };
    domwindow.addEventListener("load", onLoad, true);
  },
  onCloseWindow(aXULWindow) {},
  QueryInterface: ChromeUtils.generateQI(["nsIWindowMediatorListener"]),
};
