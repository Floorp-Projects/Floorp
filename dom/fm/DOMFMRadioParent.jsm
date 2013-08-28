/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

let DEBUG = 0;
if (DEBUG)
  debug = function(s) { dump("-*- DOMFMRadioParent component: " + s + "\n");  };
else
  debug = function(s) {};

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const MOZ_SETTINGS_CHANGED_OBSERVER_TOPIC  = "mozsettings-changed";
const PROFILE_BEFORE_CHANGE_OBSERVER_TOPIC = "profile-before-change";

const BAND_87500_108000_kHz = 1;
const BAND_76000_108000_kHz = 2;
const BAND_76000_90000_kHz  = 3;

const FM_BANDS = { };
FM_BANDS[BAND_76000_90000_kHz] = {
  lower: 76000,
  upper: 90000
};

FM_BANDS[BAND_87500_108000_kHz] = {
  lower: 87500,
  upper: 108000
};

FM_BANDS[BAND_76000_108000_kHz] = {
  lower: 76000,
  upper: 108000
};

const BAND_SETTING_KEY          = "fmRadio.band";
const CHANNEL_WIDTH_SETTING_KEY = "fmRadio.channelWidth";

// Hal types
const CHANNEL_WIDTH_200KHZ = 200;
const CHANNEL_WIDTH_100KHZ = 100;
const CHANNEL_WIDTH_50KHZ  = 50;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyGetter(this, "FMRadio", function() {
  return Cc["@mozilla.org/fmradio;1"].getService(Ci.nsIFMRadio);
});

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

this.EXPORTED_SYMBOLS = ["DOMFMRadioParent"];

this.DOMFMRadioParent = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  _initialized: false,

  /* Indicates if the FM radio is currently enabled */
  _isEnabled: false,

  /* Indicates if the FM radio is currently being enabled */
  _enabling: false,

  /* Current frequency in KHz */
  _currentFrequency: 0,

  /* Current band setting */
  _currentBand: BAND_87500_108000_kHz,

  /* Current channel width */
  _currentWidth: CHANNEL_WIDTH_100KHZ,

  /* Indicates if the antenna is currently available */
  _antennaAvailable: true,

  _seeking: false,

  _seekingCallback: null,

  init: function() {
    if (this._initialized === true) {
      return;
    }
    this._initialized = true;

    this._messages = ["DOMFMRadio:enable", "DOMFMRadio:disable",
                      "DOMFMRadio:setFrequency", "DOMFMRadio:getCurrentBand",
                      "DOMFMRadio:getPowerState", "DOMFMRadio:getFrequency",
                      "DOMFMRadio:getAntennaState",
                      "DOMFMRadio:seekUp", "DOMFMRadio:seekDown",
                      "DOMFMRadio:cancelSeek",
                      "DOMFMRadio:updateVisibility",
                     ];
    this._messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }.bind(this));

    Services.obs.addObserver(this, PROFILE_BEFORE_CHANGE_OBSERVER_TOPIC, false);
    Services.obs.addObserver(this, MOZ_SETTINGS_CHANGED_OBSERVER_TOPIC, false);

    this._updatePowerState();

    // Get the band setting and channel width setting
    let lock = gSettingsService.createLock();
    lock.get(BAND_SETTING_KEY, this);
    lock.get(CHANNEL_WIDTH_SETTING_KEY, this);

    this._updateAntennaState();

    let self = this;
    FMRadio.onantennastatechange = function onantennachange() {
      self._updateAntennaState();
    };

    debug("Initialized");
  },

  // nsISettingsServiceCallback
  handle: function(aName, aResult) {
    if (aName == BAND_SETTING_KEY) {
      this._updateBand(aResult);
    } else if (aName == CHANNEL_WIDTH_SETTING_KEY) {
      this._updateChannelWidth(aResult);
    }
  },

  handleError: function(aErrorMessage) {
    this._updateBand(BAND_87500_108000_kHz);
    this._updateChannelWidth(CHANNEL_WIDTH_100KHZ);
  },

  _updateAntennaState: function() {
    let antennaState = FMRadio.isAntennaAvailable;

    if (antennaState != this._antennaAvailable) {
      this._antennaAvailable = antennaState;
      ppmm.broadcastAsyncMessage("DOMFMRadio:antennaChange", { });
    }
  },

  _updateBand: function(band) {
      switch (parseInt(band)) {
        case BAND_87500_108000_kHz:
        case BAND_76000_108000_kHz:
        case BAND_76000_90000_kHz:
          this._currentBand = band;
          break;
      }
  },

  _updateChannelWidth: function(channelWidth) {
    switch (parseInt(channelWidth)) {
      case CHANNEL_WIDTH_50KHZ:
      case CHANNEL_WIDTH_100KHZ:
      case CHANNEL_WIDTH_200KHZ:
        this._currentWidth = channelWidth;
        break;
    }
  },

  /**
   * Update and cache the current frequency.
   * Send frequency change message if the frequency is changed.
   * The returned boolean value indicates if the frequency is changed.
   */
  _updateFrequency: function() {
    let frequency = FMRadio.frequency;

    if (frequency != this._currentFrequency) {
      this._currentFrequency = frequency;
      ppmm.broadcastAsyncMessage("DOMFMRadio:frequencyChange", { });
      return true;
    }

    return false;
  },

  /**
   * Update and cache the power state of the FM radio.
   * Send message if the power state is changed.
   */
  _updatePowerState: function() {
    let enabled = FMRadio.enabled;

    if (this._isEnabled != enabled) {
      this._isEnabled = enabled;
      ppmm.broadcastAsyncMessage("DOMFMRadio:powerStateChange", { });

      // If the FM radio is enabled, update the current frequency immediately,
      if (enabled) {
        this._updateFrequency();
      }
    }
  },

  _onSeekComplete: function(success) {
    if (this._seeking) {
      this._seeking = false;

      if (this._seekingCallback) {
        this._seekingCallback(success);
        this._seekingCallback = null;
      }
    }
  },

  /**

   * Seek the next channel with given direction.
   * Only one seek action is allowed at once.
   */
  _seekStation: function(direction, aMessage) {
    let msg = aMessage.json || { };
    let messageName = aMessage.name + ":Return";

    // If the FM radio is disabled, do not execute the seek action.
    if(!this._isEnabled) {
       this._sendMessage(messageName, false, null, msg);
       return;
    }

    let self = this;
    function callback(success) {
      debug("Seek completed.");
      if (!success) {
        self._sendMessage(messageName, false, null, msg);
      } else {
        // Make sure the FM app will get the right frequency.
        self._updateFrequency();
        self._sendMessage(messageName, true, null, msg);
      }
    }

    if (this._seeking) {
      // Pass a boolean value to the callback which indicates that
      // the seek action failed.
      callback(false);
      return;
    }

    this._seekingCallback = callback;
    this._seeking = true;

    let self = this;
    FMRadio.seek(direction);
    FMRadio.addEventListener("seekcomplete", function FM_onSeekComplete() {
      FMRadio.removeEventListener("seekcomplete", FM_onSeekComplete);
      self._onSeekComplete(true);
    });
  },

  /**
   * Round the frequency to match the range of frequency and the channel width.
   * If the given frequency is out of range, return null.
   * For example:
   *  - lower: 87.5MHz, upper: 108MHz, channel width: 0.2MHz
   *    87600 is rounded to 87700
   *    87580 is rounded to 87500
   *    109000 is not rounded, null will be returned
   */
  _roundFrequency: function(frequencyInKHz) {
    if (frequencyInKHz < FM_BANDS[this._currentBand].lower ||
        frequencyInKHz > FM_BANDS[this._currentBand].upper) {
      return null;
    }

    let partToBeRounded = frequencyInKHz - FM_BANDS[this._currentBand].lower;
    let roundedPart = Math.round(partToBeRounded / this._currentWidth) *
                        this._currentWidth;
    return FM_BANDS[this._currentBand].lower + roundedPart;
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case PROFILE_BEFORE_CHANGE_OBSERVER_TOPIC:
        this._messages.forEach(function(msgName) {
          ppmm.removeMessageListener(msgName, this);
        }.bind(this));

        Services.obs.removeObserver(this, PROFILE_BEFORE_CHANGE_OBSERVER_TOPIC);
        Services.obs.removeObserver(this, MOZ_SETTINGS_CHANGED_OBSERVER_TOPIC);

        ppmm = null;
        this._messages = null;
        break;
      case MOZ_SETTINGS_CHANGED_OBSERVER_TOPIC:
        let setting = JSON.parse(aData);
        this.handleMozSettingsChanged(setting);
        break;
    }
  },

  _sendMessage: function(message, success, data, msg) {
    msg.manager.sendAsyncMessage(message + (success ? ":OK" : ":NO"), {
      data: data,
      rid: msg.rid,
      mid: msg.mid
    });
  },

  handleMozSettingsChanged: function(settings) {
    switch (settings.key) {
      case BAND_SETTING_KEY:
        this._updateBand(settings.value);
        break;
      case CHANNEL_WIDTH_SETTING_KEY:
        this._updateChannelWidth(settings.value);
        break;
    }
  },

  _enableFMRadio: function(msg) {
    let frequencyInKHz = this._roundFrequency(msg.data * 1000);

    // If the FM radio is already enabled or it is currently being enabled
    // or the given frequency is out of range, return false.
    if (this._isEnabled || this._enabling || !frequencyInKHz) {
      this._sendMessage("DOMFMRadio:enable:Return", false, null, msg);
      return;
    }

    this._enabling = true;
    let self = this;

    FMRadio.addEventListener("enabled", function on_enabled() {
      dump("Perf:FMRadio:Enable " + (Date.now()- timeStart) + " ms.\n");
      self._enabling = false;

      FMRadio.removeEventListener("enabled", on_enabled);

      // To make sure the FM app will get right frequency after the FM
      // radio is enabled, we have to set the frequency first.
      FMRadio.setFrequency(frequencyInKHz);

      // Update the current frequency without sending 'frequencyChange'
      // msg, to make sure the FM app will get the right frequency when the
      // 'enabled' event is fired.
      self._currentFrequency = FMRadio.frequency;

      self._updatePowerState();
      self._sendMessage("DOMFMRadio:enable:Return", true, null, msg);

      // The frequency is changed from 'null' to some number, so we should
      // send the 'frequencyChange' message manually.
      ppmm.broadcastAsyncMessage("DOMFMRadio:frequencyChange", { });
    });

    let timeStart = Date.now();

    FMRadio.enable({
      lowerLimit: FM_BANDS[self._currentBand].lower,
      upperLimit: FM_BANDS[self._currentBand].upper,
      channelWidth:  self._currentWidth   // 100KHz by default
    });
  },

  _disableFMRadio: function(msg) {
    // If the FM radio is already disabled, return false.
    if (!this._isEnabled) {
      this._sendMessage("DOMFMRadio:disable:Return", false, null, msg);
      return;
    }

    let self = this;
    FMRadio.addEventListener("disabled", function on_disabled() {
      debug("FM Radio is disabled!");
      FMRadio.removeEventListener("disabled", on_disabled);

      self._updatePowerState();
      self._sendMessage("DOMFMRadio:disable:Return", true, null, msg);

      // If the FM Radio is currently seeking, no fail-to-seek or similar
      // event will be fired, execute the seek callback manually.
      self._onSeekComplete(false);
    });

    FMRadio.disable();
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json || {};
    msg.manager = aMessage.target;

    let ret = 0;
    let self = this;

    if (!aMessage.target.assertPermission("fmradio")) {
      Cu.reportError("FMRadio message " + aMessage.name +
                     " from a content process with no 'fmradio' privileges.");
      return null;
    }

    switch (aMessage.name) {
      case "DOMFMRadio:enable":
        self._enableFMRadio(msg);
        break;
      case "DOMFMRadio:disable":
        self._disableFMRadio(msg);
        break;
      case "DOMFMRadio:setFrequency":
        let frequencyInKHz = self._roundFrequency(msg.data * 1000);

        // If the FM radio is disabled or the given frequency is out of range,
        // skip to set frequency and send back the False message immediately.
        if (!self._isEnabled || !frequencyInKHz) {
          self._sendMessage("DOMFMRadio:setFrequency:Return", false, null, msg);
        } else {
          FMRadio.setFrequency(frequencyInKHz);
          self._sendMessage("DOMFMRadio:setFrequency:Return", true, null, msg);
          this._updateFrequency();
        }
        break;
      case "DOMFMRadio:getCurrentBand":
        // this message is sync
        return {
          lower: FM_BANDS[self._currentBand].lower / 1000,   // in MHz
          upper: FM_BANDS[self._currentBand].upper / 1000,   // in MHz
          channelWidth: self._currentWidth / 1000            // in MHz
        };
      case "DOMFMRadio:getPowerState":
        // this message is sync
        return self._isEnabled;
      case "DOMFMRadio:getFrequency":
        // this message is sync
        return self._isEnabled ? this._currentFrequency / 1000 : null; // in MHz
      case "DOMFMRadio:getAntennaState":
        // this message is sync
        return self._antennaAvailable;
      case "DOMFMRadio:seekUp":
        self._seekStation(Ci.nsIFMRadio.SEEK_DIRECTION_UP, aMessage);
        break;
      case "DOMFMRadio:seekDown":
        self._seekStation(Ci.nsIFMRadio.SEEK_DIRECTION_DOWN, aMessage);
        break;
      case "DOMFMRadio:cancelSeek":
        // If the FM radio is disabled, or the FM radio is not currently
        // seeking, do not execute the cancel seek action.
        if (!self._isEnabled || !self._seeking) {
          self._sendMessage("DOMFMRadio:cancelSeek:Return", false, null, msg);
        } else {
          FMRadio.cancelSeek();
          // No fail-to-seek or similar event will be fired from the hal part,
          // so execute the seek callback here manually.
          this._onSeekComplete(false);
          // The FM radio will stop at one frequency without any event, so we need to
          // update the current frequency, make sure the FM app will get the right frequency.
          this._updateFrequency();
          self._sendMessage("DOMFMRadio:cancelSeek:Return", true, null, msg);
        }
        break;
      case "DOMFMRadio:updateVisibility":
        FMRadio.updateVisible(msg == 'visible');
        break;
    }
  }
};

DOMFMRadioParent.init();

