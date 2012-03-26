/**
 * Load the browser with the given url and then invokes the given function.
 */
function openBrowserWindow(aFunc, aURL, aRect)
{
  gBrowserContext.testFunc = aFunc;
  gBrowserContext.startURL = aURL;
  gBrowserContext.browserRect = aRect;

  addLoadEvent(openBrowserWindowIntl);
}

/**
 * Close the browser window.
 */
function closeBrowserWindow()
{
  gBrowserContext.browserWnd.close();
}

/**
 * Return the browser window object.
 */
function browserWindow()
{
  return gBrowserContext.browserWnd;
}

/**
 * Return the document of the browser window.
 */
function browserDocument()
{
  return browserWindow().document;
}

/**
 * Return tab browser object.
 */
function tabBrowser()
{
  return browserWindow().gBrowser;
}

/**
 * Return browser element of the current tab.
 */
function currentBrowser()
{
  return tabBrowser().selectedBrowser;
}

/**
 * Return DOM document of the current tab.
 */
function currentTabDocument()
{
  return currentBrowser().contentDocument;
}

/**
 * Return window of the current tab.
 */
function currentTabWindow()
{
  return currentTabDocument().defaultView;
}

/**
 * Return browser element of the tab at the given index.
 */
function browserAt(aIndex)
{
  return tabBrowser().getBrowserAtIndex(aIndex);
}

/**
 * Return DOM document of the tab at the given index.
 */
function tabDocumentAt(aIndex)
{
  return browserAt(aIndex).contentDocument;
}

/**
 * Return input element of address bar.
 */
function urlbarInput()
{
  return browserWindow().document.getElementById("urlbar").inputField;
}

/**
 * Return reload button.
 */
function reloadButton()
{
  return browserWindow().document.getElementById("urlbar-reload-button");
}

////////////////////////////////////////////////////////////////////////////////
// private section

Components.utils.import("resource://gre/modules/Services.jsm");

var gBrowserContext =
{
  browserWnd: null,
  testFunc: null,
  startURL: ""
};

function openBrowserWindowIntl()
{
  var params = "chrome,all,dialog=no";
  var rect = gBrowserContext.browserRect;
  if (rect) {
    if ("left" in rect)
      params += ",left=" + rect.left;
    if ("top" in rect)
      params += ",top=" + rect.top;
    if ("width" in rect)
      params += ",width=" + rect.width;
    if ("height" in rect)
      params += ",height=" + rect.height;
  }

  gBrowserContext.browserWnd =
    window.openDialog(Services.prefs.getCharPref("browser.chromeURL"),
                      "_blank", params,
                      gBrowserContext.startURL);

  addA11yLoadEvent(startBrowserTests, browserWindow());
}

function startBrowserTests()
{
  if (gBrowserContext.startURL) // wait for load
    addA11yLoadEvent(gBrowserContext.testFunc, currentBrowser().contentWindow);
  else
    gBrowserContext.testFunc();
}
