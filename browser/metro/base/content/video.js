/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var FullScreenVideo = {
  _tab: null,

  init: function fsv_init() {
    // These come in from content.js, currently we only use Start.
    messageManager.addMessageListener("Browser:FullScreenVideo:Start", this.show.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Close", this.hide.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Play", this.play.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Pause", this.pause.bind(this));

    // If the screen supports brightness locks, we will utilize that, see checkBrightnessLocking()
    try {
      this.screen = null;
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
      this.screen = screenManager.primaryScreen;
    }
    catch (e) {} // The screen does not support brightness locks
  },

  play: function() {
    this.playing = true;
    this.checkBrightnessLocking();
  },

  pause: function() {
    this.playing = false;
    this.checkBrightnessLocking();
  },

  checkBrightnessLocking: function() {
    // screen manager support for metro: bug 776113
    var shouldLock = !!this.screen && !!window.fullScreen && !!this.playing;
    var locking = !!this.brightnessLocked;
    if (shouldLock == locking)
      return;

    if (shouldLock)
      this.screen.lockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    else
      this.screen.unlockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    this.brightnessLocked = shouldLock;
  },

  show: function fsv_show() {
    this.createTab();
    this.checkBrightnessLocking();
  },

  hide: function fsv_hide() {
    this.checkBrightnessLocking();
    this.destroyTab();
  },

  createTab: function fsv_createBrowser() {
    this._tab = BrowserUI.newTab("chrome://browser/content/fullscreen-video.xhtml");
  },

  destroyTab: function fsv_destroyBrowser() {
    Browser.closeTab(this._tab);
    this._tab = null;
  }
};
