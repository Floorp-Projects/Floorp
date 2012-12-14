/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "WebConsolePanel" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                  "resource:///modules/HUDService.jsm");

/**
 * A DevToolPanel that controls the Web Console.
 */
function WebConsolePanel(iframeWindow, toolbox) {
  this._frameWindow = iframeWindow;
  this._toolbox = toolbox;
  new EventEmitter(this);
}

WebConsolePanel.prototype = {
  /**
   * open is effectively an asynchronous constructor
   */
  open: function StyleEditor_open() {
    let tab = this._toolbox._getHostTab();
    let parentDoc = this._frameWindow.document.defaultView.parent.document;
    let iframe = parentDoc.getElementById("toolbox-panel-iframe-webconsole");
    this.hud = HUDService.activateHUDForContext(tab, iframe, this._toolbox.target);

    let deferred = Promise.defer();

    let hudId = this.hud.hudId;
    let onOpen = function _onWebConsoleOpen(aSubject) {
      aSubject.QueryInterface(Ci.nsISupportsString);
      if (hudId == aSubject.data) {
        Services.obs.removeObserver(onOpen, "web-console-created");

        this._isReady = true;
        this.emit("ready");
        deferred.resolve(this);
      }
    }.bind(this);

    Services.obs.addObserver(onOpen, "web-console-created", false);

    return deferred.promise;
  },

  get target() this._toolbox.target,

  _isReady: false,
  get isReady() this._isReady,

  destroy: function WCP_destroy()
  {
    if (this.destroyer) {
      return this.destroyer.promise;
    }

    this.destroyer = Promise.defer();

    let hudId = this.hud.hudId;

    let onClose = function _onWebConsoleClose(aSubject)
    {
      aSubject.QueryInterface(Ci.nsISupportsString);
      if (hudId == aSubject.data) {
        Services.obs.removeObserver(onClose, "web-console-destroyed");

        this.emit("destroyed");
        this.destroyer.resolve(null);
      }
    }.bind(this);

    Services.obs.addObserver(onClose, "web-console-destroyed", false);
    HUDService.deactivateHUDForContext(this.hud.tab, false);

    return this.destroyer.promise;
  },
};
