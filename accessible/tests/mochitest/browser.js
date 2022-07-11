/* import-globals-from common.js */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

/**
 * Load the browser with the given url and then invokes the given function.
 */
function openBrowserWindow(aFunc, aURL, aRect) {
  gBrowserContext.testFunc = aFunc;
  gBrowserContext.startURL = aURL;
  gBrowserContext.browserRect = aRect;

  addLoadEvent(openBrowserWindowIntl);
}

/**
 * Close the browser window.
 */
function closeBrowserWindow() {
  gBrowserContext.browserWnd.close();
}

/**
 * Return the browser window object.
 */
function browserWindow() {
  return gBrowserContext.browserWnd;
}

/**
 * Return the document of the browser window.
 */
function browserDocument() {
  return browserWindow().document;
}

/**
 * Return tab browser object.
 */
function tabBrowser() {
  return browserWindow().gBrowser;
}

/**
 * Return browser element of the current tab.
 */
function currentBrowser() {
  return tabBrowser().selectedBrowser;
}

/**
 * Return DOM document of the current tab.
 */
function currentTabDocument() {
  return currentBrowser().contentDocument;
}

/**
 * Return window of the current tab.
 */
function currentTabWindow() {
  return currentTabDocument().defaultView;
}

/**
 * Return browser element of the tab at the given index.
 */
function browserAt(aIndex) {
  return tabBrowser().getBrowserAtIndex(aIndex);
}

/**
 * Return DOM document of the tab at the given index.
 */
function tabDocumentAt(aIndex) {
  return browserAt(aIndex).contentDocument;
}

/**
 * Return input element of address bar.
 */
function urlbarInput() {
  return browserWindow().document.getElementById("urlbar").inputField;
}

/**
 * Return reload button.
 */
function reloadButton() {
  return browserWindow().document.getElementById("urlbar-reload-button");
}

// //////////////////////////////////////////////////////////////////////////////
// private section

var gBrowserContext = {
  browserWnd: null,
  testFunc: null,
  startURL: "",
};

function openBrowserWindowIntl() {
  var params = "chrome,all,dialog=no,non-remote";
  var rect = gBrowserContext.browserRect;
  if (rect) {
    if ("left" in rect) {
      params += ",left=" + rect.left;
    }
    if ("top" in rect) {
      params += ",top=" + rect.top;
    }
    if ("width" in rect) {
      params += ",width=" + rect.width;
    }
    if ("height" in rect) {
      params += ",height=" + rect.height;
    }
  }

  gBrowserContext.browserWnd = window.browsingContext.topChromeWindow.openDialog(
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    params,
    gBrowserContext.startURL || "data:text/html,<html></html>"
  );

  whenDelayedStartupFinished(browserWindow(), function() {
    addA11yLoadEvent(startBrowserTests, browserWindow());
  });
}

function startBrowserTests() {
  if (gBrowserContext.startURL) {
    // Make sure the window is the one loading our URL.
    if (currentBrowser().contentWindow.location != gBrowserContext.startURL) {
      setTimeout(startBrowserTests, 0);
      return;
    }
    // wait for load
    addA11yLoadEvent(gBrowserContext.testFunc, currentBrowser().contentWindow);
  } else {
    gBrowserContext.testFunc();
  }
}

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      setTimeout(aCallback, 0);
    }
  }, "browser-delayed-startup-finished");
}
