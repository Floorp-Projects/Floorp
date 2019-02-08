/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Services */

this.EveryWindow = {
  _callbacks: new Map(),
  _initialized: false,

  registerCallback: function EW_registerCallback(id, init, uninit) {
    if (this._callbacks.has(id)) {
      return;
    }

    this._callForEveryWindow(init);
    this._callbacks.set(id, {id, init, uninit});

    if (!this._initialized) {
      Services.obs.addObserver(this._onOpenWindow.bind(this),
                               "browser-delayed-startup-finished");
      this._initialized = true;
    }
  },

  unregisterCallback: function EW_unregisterCallback(aId, aCallUninit = true) {
    if (!this._callbacks.has(aId)) {
      return;
    }

    if (aCallUninit) {
      this._callForEveryWindow(this._callbacks.get(aId).uninit);
    }

    this._callbacks.delete(aId);
  },

  _callForEveryWindow(aFunction) {
    let windowList = Services.wm.getEnumerator("navigator:browser");
    while (windowList.hasMoreElements()) {
      let win = windowList.getNext();
      win.delayedStartupPromise.then(() => { aFunction(win); });
    }
  },

  _onOpenWindow(aWindow) {
    for (let c of this._callbacks.values()) {
      c.init(aWindow);
    }

    aWindow.addEventListener("unload",
                             this._onWindowClosing.bind(this),
                             { once: true });
  },

  _onWindowClosing(aEvent) {
    let win = aEvent.target;
    for (let c of this._callbacks.values()) {
      c.uninit(win);
    }
  },
};
