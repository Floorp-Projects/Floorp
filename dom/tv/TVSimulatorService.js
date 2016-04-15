/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function debug(aMsg) {
  //dump("[TVSimulatorService] " + aMsg + "\n");
}

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;
const Cr = Components.returnCode;

Cu.importGlobalProperties(["File"]);
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const TV_SIMULATOR_DUMMY_DIRECTORY   = "dummy";
const TV_SIMULATOR_DUMMY_FILE        = "settings.json";

// See http://seanyhlin.github.io/TV-Manager-API/#idl-def-TVSourceType
const TV_SOURCE_TYPES = ["dvb-t","dvb-t2","dvb-c","dvb-c2","dvb-s",
                         "dvb-s2","dvb-h","dvb-sh","atsc","atsc-m/h",
                         "isdb-t","isdb-tb","isdb-s","isdb-c","1seg",
                         "dtmb","cmmb","t-dmb","s-dmb"];
function containInvalidSourceType(aElement, aIndex, aArray) {
  return !TV_SOURCE_TYPES.includes(aElement);
}

// See http://seanyhlin.github.io/TV-Manager-API/#idl-def-TVChannelType
const TV_CHANNEL_TYPES = ["tv","radio","data"];

function TVSimulatorService() {
  // This is used for saving the registered source listeners. The object literal
  // is defined as below:
  // {
  //   "tunerId1": {
  //     "sourceType1": [ /* source listeners */ ],
  //     "sourceType2": [ /* source listeners */ ],
  //     ...
  //   },
  //   ...
  // }
  this._sourceListeners = {};
  this._internalTuners = null;
  this._scanCompleteTimer = null;
  this._scanningWrapTunerData = null;
  this._init();
}

TVSimulatorService.prototype = {
  classID: Components.ID("{94b065ad-d45a-436a-b394-6dabc3cf110f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITVSimulatorService,
                                         Ci.nsITVService,
                                         Ci.nsITimerCallback]),

  _init: function TVSimInit() {
    if (this._internalTuners) {
      return;
    }

    // I try to load the testing mock data if related preference are already set.
    // Otherwise, use to the simulation data from prefences.
    // See /dom/tv/test/mochitest/head.js for more details.
    let settingStr = "";
    try {
      settingStr = Services.prefs.getCharPref("dom.testing.tv_mock_data");
    } catch(e) {
      try {
        settingStr = this._getDummyData();
      } catch(e) {
        debug("TV Simulator service failed to load simulation data: " + e);
        return;
      }
    }

    let settingsObj;
    try {
      /*
       *
       * Setting JSON file format:
       *
       * Note: This setting JSON is not allow empty array.
       *       If set the empty array, _init() will fail.
       *       e.g.
       *        - "tuners": []
       *        - "channels":[]
       * Format:
       *   {
       *    "tuners": [{
       *      "id":                     "The ID of the tuner",
       *      "supportedType":          ["The array of source type to be used."],
       *      "sources": [{
       *        "type":                 "The source type to be used",
       *        "channels" : [{
       *          "networkId":          "The ID of the channel network",
       *          "transportStreamId":  "The ID of channel transport stream",
       *          "serviceId":          "The ID of channel service",
       *          "type":               "The type of channel",
       *          "name":               "The channel name",
       *          "number" :            The LCN (Logical Channel Number) of the channel,
       *          "isEmergency" :       Whether this channel is emergency status,
       *          "isFree":             Whether this channel is free or not,
       *          "videoFilePath":      "The path of the fake video file",
       *          "programs":[{
       *            "eventId":          "The ID of this program event",
       *            "title" :           "This program's title",
       *            "startTime":        "The start time of this program",
       *            "duration":         "The duration of this program",
       *            "description":      "The description of this program",
       *            "rating":           "The rating of this program",
       *            "audioLanugages":   ["The array of audio language"],
       *            "subtitleLanguages":["The array of subtitle language"],
       *           },]
       *         },]
       *       },]
       *     },]
       *   }
       */
      settingsObj = JSON.parse(settingStr);
    } catch(e) {
      debug("File load error: " + e);
      return;
    }

    // validation
    if (!this._validateSettings(settingsObj)) {
      debug("Failed to validate settings.");
      return;
    }

    // Key is as follow
    // {'tunerId':tunerId, 'sourceType':sourceType}
    this._internalTuners = new Map();

    // TVTunerData
    for (let tunerData of settingsObj.tuners) {
      let tuner = Cc["@mozilla.org/tv/tvtunerdata;1"]
                    .createInstance(Ci.nsITVTunerData);
      tuner.id = tunerData.id;
      tuner.streamType = tuner.TV_STREAM_TYPE_SIMULATOR;
      tuner.setSupportedSourceTypes(tunerData.supportedType.length,
                                    tunerData.supportedType);

      let wrapTunerData = {
        'tuner': tuner,
        'channels': new Map(),
        'sourceType': undefined,
      };

      // TVSource
      for (let sourceData of tunerData.sources) {
        wrapTunerData.sourceType = sourceData.type;

        // TVChannel
        for (let channelData of sourceData.channels) {
          let channel = Cc["@mozilla.org/tv/tvchanneldata;1"]
                          .createInstance(Ci.nsITVChannelData);
          channel.networkId         = channelData.networkId;
          channel.transportStreamId = channelData.transportStreamId;
          channel.serviceId         = channelData.serviceId;
          channel.type              = channelData.type;
          channel.name              = channelData.name;
          channel.number            = channelData.number;
          channel.isEmergency       = channelData.isEmergency;
          channel.isFree            = channelData.isFree;

          let wrapChannelData = {
            'channel': channel,
            'programs': new Array(),
            'videoFilePath': channelData.videoFilePath,
          };

          // TVProgram
          for (let programData of channelData.programs) {
            let program = Cc["@mozilla.org/tv/tvprogramdata;1"]
                            .createInstance(Ci.nsITVProgramData);
            program.eventId     = programData.eventId;
            program.title       = programData.title;
            program.startTime   = programData.startTime;
            program.duration    = programData.duration;
            program.description = programData.description;
            program.rating      = programData.rating;
            program.setAudioLanguages(programData.audioLanguages.length,
                                      programData.audioLanguages);
            program.setSubtitleLanguages(programData.subtitleLanguages.length,
                                         programData.subtitleLanguages);
            wrapChannelData.programs.push(program);
          }

          // Sort the program according to the startTime
          wrapChannelData.programs.sort(function(a, b) {
            return a.startTime - b.startTime;
          });
          wrapTunerData.channels.set(channel.number, wrapChannelData);
        }

        // Sort the channel according to the channel number
        wrapTunerData.channels = new Map([...wrapTunerData.channels.entries()].sort(function(a, b) {
          return a[0] - b[0];
        }));
        this._internalTuners.set(
                this._getTunerMapKey(tuner.id, sourceData.type),
                wrapTunerData);
      }
    }
  },

  registerSourceListener: function(aTunerId, aSourceType, aListener) {
    let tunerSourceListeners = this._sourceListeners[aTunerId];
    if (!tunerSourceListeners) {
      tunerSourceListeners = this._sourceListeners[aTunerId] = {};
    }

    let listeners = tunerSourceListeners[aSourceType];
    if (!listeners) {
      listeners = tunerSourceListeners[aSourceType] = [];
    }

    if (listeners.indexOf(aListener) < 0) {
      listeners.push(aListener);
    }
  },

  unregisterSourceListener: function(aTunerId, aSourceType, aListener) {
    let tunerSourceListeners = this._sourceListeners[aTunerId];
    if (!tunerSourceListeners) {
      return;
    }

    let listeners = tunerSourceListeners[aSourceType];
    if (!listeners) {
      return;
    }

    let index = listeners.indexOf(aListener);
    if (index < 0) {
      return;
    }

    listeners.splice(index, 1);
  },

  _getSourceListeners: function(aTunerId, aSourceType) {
    let tunerSourceListeners = this._sourceListeners[aTunerId];
    if (!tunerSourceListeners) {
      return [];
    }

    let listeners = tunerSourceListeners[aSourceType];
    if (!listeners) {
      return [];
    }

    return listeners;
  },

  getTuners: function(aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let tuners = Cc["@mozilla.org/array;1"]
                   .createInstance(Ci.nsIMutableArray);

    for (let [k,wrapTunerData] of this._internalTuners) {
      tuners.appendElement(wrapTunerData.tuner, false);
    }

    aCallback.notifySuccess(tuners);
  },

  setSource: function(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw NS_ERROR_INVALID_ARG;
    }

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    let streamHandle = Cc["@mozilla.org/tv/tvgonknativehandledata;1"]
                         .createInstance(Ci.nsITVGonkNativeHandleData);
    let streamHandles = Cc["@mozilla.org/array;1"]
                          .createInstance(Ci.nsIMutableArray);
    streamHandles.appendElement(streamHandle, false);

    aCallback.notifySuccess(streamHandles);
  },

  startScanningChannels: function(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    if (this._scanningWrapTunerData) {
      aCallback.notifyError(Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    this._scanningWrapTunerData = wrapTunerData;

    aCallback.notifySuccess(null);

    for (let [key, wrapChannelData] of wrapTunerData.channels) {
      for (let listener of this._getSourceListeners(aTunerId, aSourceType)) {
        listener.notifyChannelScanned(aTunerId, aSourceType, wrapChannelData.channel);
      }
    }

    this._scanCompleteTimer = Cc["@mozilla.org/timer;1"]
                                .createInstance(Ci.nsITimer);
    this._scanCompleteTimer.initWithCallback(this, 10,
                                             Ci.nsITimer.TYPE_ONE_SHOT);
  },

  notify: function(aTimer) {
    if (!this._scanningWrapTunerData) {
      return;
    }

    this._scanCompleteTimer = null;

    let tunerId = this._scanningWrapTunerData.tuner.id;
    let sourceType = this._scanningWrapTunerData.sourceType;
    let notifyResult = Cr.NS_OK;
    for (let listener of this._getSourceListeners(tunerId, sourceType)) {
      notifyResult = listener.notifyChannelScanComplete(tunerId, sourceType);
    }
    this._scanningWrapTunerData = null;
    return notifyResult;
  },

  stopScanningChannels: function(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    if (!this._scanningWrapTunerData ||
        aTunerId != this._scanningWrapTunerData.tuner.id ||
        aSourceType != this._scanningWrapTunerData.sourceType) {
      aCallback.notifyError(Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    this._scanningWrapTunerData = null;

    if (this._scanCompleteTimer) {
      this._scanCompleteTimer.cancel();
      this._scanCompleteTimer = null;
    }

    for (let listener of this._getSourceListeners(aTunerId, aSourceType)) {
      listener.notifyChannelScanStopped(aTunerId, aSourceType);
    }

    aCallback.notifySuccess(null);
  },

  clearScannedChannelsCache: function(aTunerId, aSourceType, aCallback) {
    // Simulator service doesn't support channel cache, so there's nothing to do here.
  },

  setChannel: function(aTunerId, aSourceType, aChannelNumber, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let channel = Cc["@mozilla.org/array;1"]
                    .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    let wrapChannelData = wrapTunerData.channels.get(aChannelNumber);
    if (!wrapChannelData) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    channel.appendElement(wrapChannelData.channel, false);
    aCallback.notifySuccess(channel);
  },

  getChannels: function(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let channelArray = Cc["@mozilla.org/array;1"]
                         .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    for (let [key, wrapChannelData] of wrapTunerData.channels) {
      channelArray.appendElement(wrapChannelData.channel, false);
    }

    aCallback.notifySuccess(channelArray);
  },

  getPrograms: function(aTunerId, aSourceType, aChannelNumber, aStartTime, aEndTime, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    let programArray = Cc["@mozilla.org/array;1"]
                         .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
      return;
    }

    let wrapChannelData = wrapTunerData.channels.get(aChannelNumber);
    if (!wrapChannelData || !wrapChannelData.programs) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    for (let program of wrapChannelData.programs) {
      programArray.appendElement(program, false);
    }

    aCallback.notifySuccess(programArray);
  },

  getSimulatorVideoBlobURL: function(aTunerId, aSourceType, aChannelNumber, aWin) {
    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      return "";
    }

    let wrapChannelData = wrapTunerData.channels.get(aChannelNumber);
    if (!wrapChannelData || !wrapChannelData.videoFilePath) {
      return "";
    }

    let videoFile = new File(this._getFilePath(wrapChannelData.videoFilePath));
    let videoBlobURL = aWin.URL.createObjectURL(videoFile);

    return videoBlobURL;
  },

  _getDummyData : function() {
    // Load the setting file from local JSON file.
    // Synchrhronous File Reading.
    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);

    let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                    .createInstance(Ci.nsIFileInputStream);
    let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                   .createInstance(Ci.nsIConverterInputStream);

    let settingsStr = "";

    try {
      file.initWithPath(this._getFilePath(TV_SIMULATOR_DUMMY_FILE));
      fstream.init(file, -1, 0, 0);
      cstream.init(fstream,
                   "UTF-8",
                   1024,
                   Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

      let str = {};
      while (cstream.readString(0xffffffff, str) != 0) {
        settingsStr += str.value;
      }
    } catch(e) {
      debug("Catch the Exception when reading the dummy file:" + e );
      throw e;
    } finally {
      cstream.close();
    }

    return settingsStr;
  },

  _getTunerMapKey: function(aTunerId, aSourceType) {
    return JSON.stringify({'tunerId': aTunerId, 'sourceType': aSourceType});
  },

  _getWrapTunerData: function(aTunerId, aSourceType) {
    if (!this._internalTuners || this._internalTuners.size <= 0) {
      return null;
    }
    return this._internalTuners.get(this._getTunerMapKey(aTunerId, aSourceType));
  },

  _getFilePath: function(fileName) {
    let dsFile = Cc["@mozilla.org/file/directory_service;1"]
                   .getService(Ci.nsIProperties)
                   .get("ProfD", Ci.nsIFile);
    dsFile.append(TV_SIMULATOR_DUMMY_DIRECTORY);
    dsFile.append(fileName);

    return dsFile.path;
  },

  _validateSettings: function(aSettingsObject) {
    return this._validateTuners(aSettingsObject.tuners);
  },

  _validateTuners: function(aTunersObject) {
    let tunerIds = new Array();
    for (let tuner of aTunersObject) {
      if (!tuner.id ||
          !tuner.supportedType ||
          !tuner.supportedType.length ||
          tuner.supportedType.some(containInvalidSourceType) ||
          tunerIds.includes(tuner.id)) {
        debug("invalid tuner data.");
        return false;
      }
      tunerIds.push(tuner.id);

      if (!this._validateSources(tuner.sources)) {
        return false;
      }
    }
    return true;
  },

  _validateSources: function(aSourcesObject) {
    for (let source of aSourcesObject) {
      if (!source.type ||
          !TV_SOURCE_TYPES.includes(source.type)) {
        debug("invalid source data.");
        return false;
      }

      if (!this._validateChannels(source.channels)) {
        return false;
      }
    }
    return true;
  },

  _validateChannels: function(aChannelsObject) {
    let channelNumbers = new Array();
    for (let channel of aChannelsObject) {
      if (!channel.networkId ||
          !channel.transportStreamId ||
          !channel.serviceId ||
          !channel.type ||
          !TV_CHANNEL_TYPES.includes(channel.type) ||
          !channel.number ||
          channelNumbers.includes(channel.number) ||
          !channel.name) {
        debug("invalid channel data.");
        return false;
      }
      channelNumbers.push(channel.number);

      if (!this._validatePrograms(channel.programs)) {
        return false;
      }
    }
    return true;
  },

  _validatePrograms: function(aProgramsObject) {
    let eventIds = new Array();
    for (let program of aProgramsObject) {
      if (!program.eventId ||
          eventIds.includes(program.eventId) ||
          !program.title ||
          !program.startTime ||
          !program.duration) {
        debug("invalid program data.");
        return false;
      }
      eventIds.push(program.eventId);
    }
    return true;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TVSimulatorService]);
