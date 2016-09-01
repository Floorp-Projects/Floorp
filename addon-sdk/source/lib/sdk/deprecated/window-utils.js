/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'deprecated'
};

const { Cc, Ci } = require('chrome');
const events = require('../system/events');
const { getInnerId, getOuterId, windows, isDocumentLoaded, isBrowser,
        getMostRecentBrowserWindow, getToplevelWindow, getMostRecentWindow } = require('../window/utils');
const { deprecateFunction } = require('../util/deprecate');
const { ignoreWindow } = require('sdk/private-browsing/utils');
const { isPrivateBrowsingSupported } = require('../self');

const windowWatcher = Cc['@mozilla.org/embedcomp/window-watcher;1'].
                       getService(Ci.nsIWindowWatcher);
const appShellService = Cc['@mozilla.org/appshell/appShellService;1'].
                        getService(Ci.nsIAppShellService);

// Bug 834961: ignore private windows when they are not supported
function getWindows() {
  return windows(null, { includePrivate: isPrivateBrowsingSupported });
}

/**
 * An iterator for XUL windows currently in the application.
 *
 * @return A generator that yields XUL windows exposing the
 *         nsIDOMWindow interface.
 */
function windowIterator() {
  // Bug 752631: We only pass already loaded window in order to avoid
  // breaking XUL windows DOM. DOM is broken when some JS code try
  // to access DOM during "uninitialized" state of the related document.
  let list = getWindows().filter(isDocumentLoaded);
  for (let i = 0, l = list.length; i < l; i++) {
    yield list[i];
  }
};
exports.windowIterator = windowIterator;

/**
 * An iterator for browser windows currently open in the application.
 * @returns {Function}
 *    A generator that yields browser windows exposing the `nsIDOMWindow`
 *    interface.
 */
function browserWindowIterator() {
  for (let window of windowIterator()) {
    if (isBrowser(window))
      yield window;
  }
}
exports.browserWindowIterator = browserWindowIterator;

function WindowTracker(delegate) {
   if (!(this instanceof WindowTracker)) {
     return new WindowTracker(delegate);
   }

  this._delegate = delegate;

  for (let window of getWindows())
    this._regWindow(window);
  windowWatcher.registerNotification(this);
  this._onToplevelWindowReady = this._onToplevelWindowReady.bind(this);
  events.on('toplevel-window-ready', this._onToplevelWindowReady);

  require('../system/unload').ensure(this);

  return this;
};

WindowTracker.prototype = {
  _regLoadingWindow: function _regLoadingWindow(window) {
    // Bug 834961: ignore private windows when they are not supported
    if (ignoreWindow(window))
      return;

    window.addEventListener('load', this, true);
  },

  _unregLoadingWindow: function _unregLoadingWindow(window) {
    // This may have no effect if we ignored the window in _regLoadingWindow().
    window.removeEventListener('load', this, true);
  },

  _regWindow: function _regWindow(window) {
    // Bug 834961: ignore private windows when they are not supported
    if (ignoreWindow(window))
      return;

    if (window.document.readyState == 'complete') {
      this._unregLoadingWindow(window);
      this._delegate.onTrack(window);
    } else
      this._regLoadingWindow(window);
  },

  _unregWindow: function _unregWindow(window) {
    if (window.document.readyState == 'complete') {
      if (this._delegate.onUntrack)
        this._delegate.onUntrack(window);
    } else {
      this._unregLoadingWindow(window);
    }
  },

  unload: function unload() {
    windowWatcher.unregisterNotification(this);
    events.off('toplevel-window-ready', this._onToplevelWindowReady);
    for (let window of getWindows())
      this._unregWindow(window);
  },

  handleEvent: function handleEvent(event) {
    try {
      if (event.type == 'load' && event.target) {
        var window = event.target.defaultView;
        if (window)
          this._regWindow(getToplevelWindow(window));
      }
    }
    catch(e) {
      console.exception(e);
    }
  },

  _onToplevelWindowReady: function _onToplevelWindowReady({subject}) {
    let window = getToplevelWindow(subject);
    // ignore private windows if they are not supported
    if (ignoreWindow(window))
      return;
    this._regWindow(window);
  },

  observe: function observe(subject, topic, data) {
    try {
      var window = subject.QueryInterface(Ci.nsIDOMWindow);
      // ignore private windows if they are not supported
      if (ignoreWindow(window))
        return;
      if (topic == 'domwindowclosed')
      this._unregWindow(window);
    }
    catch(e) {
      console.exception(e);
    }
  }
};
exports.WindowTracker = WindowTracker;

Object.defineProperties(exports, {
  activeWindow: {
    enumerable: true,
    get: function() {
      return getMostRecentWindow(null);
    },
    set: function(window) {
      try {
        window.focus();
      } catch (e) {}
    }
  },
  activeBrowserWindow: {
    enumerable: true,
    get: getMostRecentBrowserWindow
  }
});


/**
 * Returns the ID of the window's current inner window.
 */
exports.getInnerId = deprecateFunction(getInnerId,
  'require("window-utils").getInnerId is deprecated, ' +
  'please use require("sdk/window/utils").getInnerId instead'
);

exports.getOuterId = deprecateFunction(getOuterId,
  'require("window-utils").getOuterId is deprecated, ' +
  'please use require("sdk/window/utils").getOuterId instead'
);

exports.isBrowser = deprecateFunction(isBrowser,
  'require("window-utils").isBrowser is deprecated, ' +
  'please use require("sdk/window/utils").isBrowser instead'
);

exports.hiddenWindow = appShellService.hiddenDOMWindow;
