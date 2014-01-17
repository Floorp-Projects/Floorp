/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { Cc, Ci } = require('chrome');
const array = require('../util/array');
const { defer } = require('sdk/core/promise');

const windowWatcher = Cc['@mozilla.org/embedcomp/window-watcher;1'].
                       getService(Ci.nsIWindowWatcher);
const appShellService = Cc['@mozilla.org/appshell/appShellService;1'].
                        getService(Ci.nsIAppShellService);
const WM = Cc['@mozilla.org/appshell/window-mediator;1'].
           getService(Ci.nsIWindowMediator);
const io = Cc['@mozilla.org/network/io-service;1'].
           getService(Ci.nsIIOService);

const XUL_NS = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

const BROWSER = 'navigator:browser',
      URI_BROWSER = 'chrome://browser/content/browser.xul',
      NAME = '_blank',
      FEATURES = 'chrome,all,dialog=no,non-private';

function isWindowPrivate(win) {
  if (!win)
    return false;

  // if the pbService is undefined, the PrivateBrowsingUtils.jsm is available,
  // and the app is Firefox, then assume per-window private browsing is
  // enabled.
  try {
    return win.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsILoadContext)
                  .usePrivateBrowsing;
  }
  catch(e) {}

  // Sometimes the input is not a nsIDOMWindow.. but it is still a winodw.
  try {
    return !!win.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing;
  }
  catch (e) {}

  return false;
}
exports.isWindowPrivate = isWindowPrivate;

function getMostRecentBrowserWindow() {
  return getMostRecentWindow(BROWSER);
}
exports.getMostRecentBrowserWindow = getMostRecentBrowserWindow;

function getHiddenWindow() {
  return appShellService.hiddenDOMWindow;
}
exports.getHiddenWindow = getHiddenWindow;

function getMostRecentWindow(type) {
  return WM.getMostRecentWindow(type);
}
exports.getMostRecentWindow = getMostRecentWindow;

/**
 * Returns the ID of the window's current inner window.
 */
function getInnerId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
};
exports.getInnerId = getInnerId;

/**
 * Returns the ID of the window's outer window.
 */
function getOuterId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
};
exports.getOuterId = getOuterId;

/**
 * Returns window by the outer window id.
 */
const getByOuterId = WM.getOuterWindowWithId;
exports.getByOuterId = getByOuterId;

const getByInnerId = WM.getCurrentInnerWindowWithId;
exports.getByInnerId = getByInnerId;

/**
 * Returns `nsIXULWindow` for the given `nsIDOMWindow`.
 */
function getXULWindow(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebNavigation).
    QueryInterface(Ci.nsIDocShellTreeItem).
    treeOwner.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIXULWindow);
};
exports.getXULWindow = getXULWindow;

function getDOMWindow(xulWindow) {
  return xulWindow.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIDOMWindow);
}
exports.getDOMWindow = getDOMWindow;

/**
 * Returns `nsIBaseWindow` for the given `nsIDOMWindow`.
 */
function getBaseWindow(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebNavigation).
    QueryInterface(Ci.nsIDocShell).
    QueryInterface(Ci.nsIDocShellTreeItem).
    treeOwner.
    QueryInterface(Ci.nsIBaseWindow);
}
exports.getBaseWindow = getBaseWindow;

/**
 * Returns the `nsIDOMWindow` toplevel window for any child/inner window
 */
function getToplevelWindow(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShellTreeItem)
               .rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindow);
}
exports.getToplevelWindow = getToplevelWindow;

function getWindowDocShell(window) window.gBrowser.docShell;
exports.getWindowDocShell = getWindowDocShell;

function getWindowLoadingContext(window) {
  return getWindowDocShell(window).
         QueryInterface(Ci.nsILoadContext);
}
exports.getWindowLoadingContext = getWindowLoadingContext;

const isTopLevel = window => window && getToplevelWindow(window) === window;
exports.isTopLevel = isTopLevel;

/**
 * Takes hash of options and serializes it to a features string that
 * can be used passed to `window.open`. For more details on features string see:
 * https://developer.mozilla.org/en/DOM/window.open#Position_and_size_features
 */
function serializeFeatures(options) {
  return Object.keys(options).reduce(function(result, name) {
    let value = options[name];

    // the chrome and private features are special
    if ((name == 'private' || name == 'chrome'))
      return result + ((value === true) ? ',' + name : '');

    return result + ',' + name + '=' +
           (value === true ? 'yes' : value === false ? 'no' : value);
  }, '').substr(1);
}

/**
 * Opens a top level window and returns it's `nsIDOMWindow` representation.
 * @params {String} uri
 *    URI of the document to be loaded into window.
 * @params {nsIDOMWindow} options.parent
 *    Used as parent for the created window.
 * @params {String} options.name
 *    Optional name that is assigned to the window.
 * @params {Object} options.features
 *    Map of key, values like: `{ width: 10, height: 15, chrome: true, private: true }`.
 */
function open(uri, options) {
  uri = uri || URI_BROWSER;
  options = options || {};

  if (['chrome', 'resource', 'data'].indexOf(io.newURI(uri, null, null).scheme) < 0)
    throw new Error('only chrome, resource and data uris are allowed');

  let newWindow = windowWatcher.
    openWindow(options.parent || null,
               uri,
               options.name || null,
               options.features ? serializeFeatures(options.features) : null,
               options.args || null);

  return newWindow;
}
exports.open = open;

function onFocus(window) {
  let { resolve, promise } = defer();

  if (isFocused(window)) {
    resolve(window);
  }
  else {
    window.addEventListener("focus", function focusListener() {
      window.removeEventListener("focus", focusListener, true);
      resolve(window);
    }, true);
  }

  return promise;
}
exports.onFocus = onFocus;

function isFocused(window) {
  const FM = Cc["@mozilla.org/focus-manager;1"].
                getService(Ci.nsIFocusManager);

  let childTargetWindow = {};
  FM.getFocusedElementForWindow(window, true, childTargetWindow);
  childTargetWindow = childTargetWindow.value;

  let focusedChildWindow = {};
  if (FM.activeWindow) {
    FM.getFocusedElementForWindow(FM.activeWindow, true, focusedChildWindow);
    focusedChildWindow = focusedChildWindow.value;
  }

  return (focusedChildWindow === childTargetWindow);
}
exports.isFocused = isFocused;

/**
 * Opens a top level window and returns it's `nsIDOMWindow` representation.
 * Same as `open` but with more features
 * @param {Object} options
 *
 */
function openDialog(options) {
  options = options || {};

  let features = options.features || FEATURES;
  let featureAry = features.toLowerCase().split(',');

  if (!!options.private) {
    // add private flag if private window is desired
    if (!array.has(featureAry, 'private')) {
      featureAry.push('private');
    }

    // remove the non-private flag ig a private window is desired
    let nonPrivateIndex = featureAry.indexOf('non-private');
    if (nonPrivateIndex >= 0) {
      featureAry.splice(nonPrivateIndex, 1);
    }

    features = featureAry.join(',');
  }

  let browser = getMostRecentBrowserWindow();

  // if there is no browser then do nothing
  if (!browser)
    return undefined;

  let newWindow = browser.openDialog.apply(
      browser,
      array.flatten([
        options.url || URI_BROWSER,
        options.name || NAME,
        features,
        options.args || null
      ])
  );

  return newWindow;
}
exports.openDialog = openDialog;

/**
 * Returns an array of all currently opened windows.
 * Note that these windows may still be loading.
 */
function windows(type, options) {
  options = options || {};
  let list = [];
  let winEnum = WM.getEnumerator(type);
  while (winEnum.hasMoreElements()) {
    let window = winEnum.getNext().QueryInterface(Ci.nsIDOMWindow);
    // Only add non-private windows when pb permission isn't set,
    // unless an option forces the addition of them.
    if (!window.closed && (options.includePrivate || !isWindowPrivate(window))) {
      list.push(window);
    }
  }
  return list;
}
exports.windows = windows;

/**
 * Check if the given window is interactive.
 * i.e. if its "DOMContentLoaded" event has already been fired.
 * @params {nsIDOMWindow} window
 */
const isInteractive = window =>
  window.document.readyState === "interactive" ||
  isDocumentLoaded(window) ||
  // XUL documents stays '"uninitialized"' until it's `readyState` becomes
  // `"complete"`.
  isXULDocumentWindow(window) && window.document.readyState === "interactive";
exports.isInteractive = isInteractive;

const isXULDocumentWindow = ({document}) =>
  document.documentElement &&
  document.documentElement.namespaceURI === XUL_NS;

/**
 * Check if the given window is completely loaded.
 * i.e. if its "load" event has already been fired and all possible DOM content
 * is done loading (the whole DOM document, images content, ...)
 * @params {nsIDOMWindow} window
 */
function isDocumentLoaded(window) {
  return window.document.readyState == "complete";
}
exports.isDocumentLoaded = isDocumentLoaded;

function isBrowser(window) {
  try {
    return window.document.documentElement.getAttribute("windowtype") === BROWSER;
  }
  catch (e) {}
  return false;
};
exports.isBrowser = isBrowser;

function getWindowTitle(window) {
  return window && window.document ? window.document.title : null;
}
exports.getWindowTitle = getWindowTitle;

function isXULBrowser(window) {
  return !!(isBrowser(window) && window.XULBrowserWindow);
}
exports.isXULBrowser = isXULBrowser;

/**
 * Returns the most recent focused window
 */
function getFocusedWindow() {
  let window = WM.getMostRecentWindow(BROWSER);

  return window ? window.document.commandDispatcher.focusedWindow : null;
}
exports.getFocusedWindow = getFocusedWindow;

/**
 * Returns the focused element in the most recent focused window
 */
function getFocusedElement() {
  let window = WM.getMostRecentWindow(BROWSER);

  return window ? window.document.commandDispatcher.focusedElement : null;
}
exports.getFocusedElement = getFocusedElement;

function getFrames(window) {
  return Array.slice(window.frames).reduce(function(frames, frame) {
    return frames.concat(frame, getFrames(frame));
  }, []);
}
exports.getFrames = getFrames;

function getScreenPixelsPerCSSPixel(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils).screenPixelsPerCSSPixel;
}
exports.getScreenPixelsPerCSSPixel = getScreenPixelsPerCSSPixel;

function getOwnerBrowserWindow(node) {
  /**
  Takes DOM node and returns browser window that contains it.
  **/
  let window = getToplevelWindow(node.ownerDocument.defaultView);
  // If anchored window is browser then it's target browser window.
  return isBrowser(window) ? window : null;
}
exports.getOwnerBrowserWindow = getOwnerBrowserWindow;

function getParentWindow(window) {
  try {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem).parent
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);
  }
  catch (e) {}
  return null;
}
exports.getParentWindow = getParentWindow;


function getParentFrame(window) {
  try {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem).parent
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);
  }
  catch (e) {}
  return null;
}
exports.getParentWindow = getParentWindow;

// The element in which the window is embedded, or `null`
// if the window is top-level. Similar to `window.frameElement`
// but can cross chrome-content boundries.
const getFrameElement = target =>
  (target instanceof Ci.nsIDOMDocument ? target.defaultView : target).
  QueryInterface(Ci.nsIInterfaceRequestor).
  getInterface(Ci.nsIDOMWindowUtils).
  containerElement;
exports.getFrameElement = getFrameElement;
