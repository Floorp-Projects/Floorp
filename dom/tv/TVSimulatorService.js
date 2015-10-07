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
const TV_SIMULATOR_DUMMY_DIRECTORY = "dummy";
const TV_SIMULATOR_DUMMY_FILE      = "settings.json";

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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function TVSimulatorService() {
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

    // Load the setting file from local JSON file.
    // Synchrhronous File Reading.
    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);

    file.initWithPath(this._getFilePath(TV_SIMULATOR_DUMMY_FILE));

    let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                    .createInstance(Ci.nsIFileInputStream);
    let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                    .createInstance(Ci.nsIConverterInputStream);

    let settingStr = "";

    try {
      fstream.init(file, -1, 0, 0);
      cstream.init(fstream,
                   "UTF-8",
                   1024,
                   Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

      let str = {};
      while (cstream.readString(0xffffffff, str) != 0) {
        settingStr += str.value;
      }
    } catch(e) {
      debug("Error occurred : " + e );
      return;
    } finally {
      cstream.close();
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
    for each (let tunerData in settingsObj.tuners) {
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
      for each (let sourceData in tunerData.sources) {
        wrapTunerData.sourceType = sourceData.type;

        // TVChannel
        for each (let channelData in sourceData.channels) {
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
          for each (let programData in channelData.programs) {
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

  getTuners: function TVSimGetTuners(aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let tuners = Cc["@mozilla.org/array;1"]
                   .createInstance(Ci.nsIMutableArray);

    for (let [k,wrapTunerData] of this._internalTuners) {
      tuners.appendElement(wrapTunerData.tuner, false);
    }

    return aCallback.notifySuccess(tuners);
  },

  setSource: function TVSimSetSource(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return NS_ERROR_INVALID_ARG;
    }

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }
    return aCallback.notifySuccess(null);
  },

  startScanningChannels: function TVSimStartScanningChannels(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    if (this._scanningWrapTunerData) {
      return aCallback.notifyError(Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    this._scanningWrapTunerData = wrapTunerData;

    aCallback.notifySuccess(null);

    for (let [key, wrapChannelData] of wrapTunerData.channels) {
      this._sourceListener.notifyChannelScanned(
                                        wrapTunerData.tuner.id,
                                        wrapTunerData.sourceType,
                                        wrapChannelData.channel);
    }

    this._scanCompleteTimer = Cc["@mozilla.org/timer;1"]
                                .createInstance(Ci.nsITimer);
    rv = this._scanCompleteTimer.initWithCallback(this, 10,
                                                  Ci.nsITimer.TYPE_ONE_SHOT);
    return Cr.NS_OK;
  },

  notify: function TVSimTimerCallback(aTimer) {
    if (!this._scanningWrapTunerData) {
      return;
    }

    this._scanCompleteTimer = null;
    this._scanningWrapTunerData = null;
    return this._sourceListener.notifyChannelScanComplete(
                                       this._scanningWrapTunerData.tuner.id,
                                       this._scanningWrapTunerData.sourceType);
  },

  stopScanningChannels: function TVSimStopScanningChannels(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    if (!this._scanningWrapTunerData) {
      return aCallback.notifyError(Cr.NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    if (this._scanCompleteTimer) {
      this._scanCompleteTimer.cancel();
      this._scanCompleteTimer = null;
    }

    if (wrapTunerData.tuner.id === this._scanningWrapTunerData.tuner.id &&
        wrapTunerData.sourceType === this._scanningWrapTunerData.sourceType) {
      this._scanningWrapTunerData = null;
      this._sourceListener.notifyChannelScanStopped(
                                 wrapTunerData.tuner.id,
                                 wrapTunerData.sourceType);
    }

    return aCallback.notifySuccess(null);
  },

  clearScannedChannelsCache: function TVSimClearScannedChannelsCache(aTunerId, aSourceType, aCallback) {
    // Doesn't support for this method.
    return Cr.NS_OK;
  },

  setChannel: function TVSimSetChannel(aTunerId, aSourceType, aChannelNumber, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let channel = Cc["@mozilla.org/array;1"]
                    .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    let wrapChannelData = wrapTunerData.channels.get(aChannelNumber);
    if (!wrapChannelData) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    channel.appendElement(wrapChannelData.channel, false);
    return aCallback.notifySuccess(channel);

  },

  getChannels: function TVSimGetChannels(aTunerId, aSourceType, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let channelArray = Cc["@mozilla.org/array;1"]
                         .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    for (let [key, wrapChannelData] of wrapTunerData.channels) {
      channelArray.appendElement(wrapChannelData.channel, false);
    }

    return aCallback.notifySuccess(channelArray);
  },

  getPrograms: function TVSimGetPrograms(aTunerId, aSourceType, aChannelNumber, aStartTime, aEndTime, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }
    let programArray = Cc["@mozilla.org/array;1"]
                         .createInstance(Ci.nsIMutableArray);

    let wrapTunerData = this._getWrapTunerData(aTunerId, aSourceType);
    if (!wrapTunerData || !wrapTunerData.channels) {
      return aCallback.notifyError(Ci.nsITVServiceCallback.TV_ERROR_FAILURE);
    }

    let wrapChannelData = wrapTunerData.channels.get(aChannelNumber);
    if (!wrapChannelData || !wrapChannelData.programs) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    for each (let program in wrapChannelData.programs) {
      programArray.appendElement(program, false);
    }

    return aCallback.notifySuccess(programArray);

  },

  getOverlayId: function TVSimGetOverlayId(aTunerId, aCallback) {
    if (!aCallback) {
      debug("aCallback is null\n");
      return Cr.NS_ERROR_INVALID_ARG;
    }

    // TVSimulatorService does not use this parameter.
    overlayIds = Cc["@mozilla.org/array;1"]
                  .createInstance(Ci.nsIMutableArray);
    return aCallback.notifySuccess(overlayIds);
  },

  set sourceListener(aListener) {
    this._sourceListener = aListener;
  },

  get sourceListener() {
      return this._sourceListener;
  },

  getSimulatorVideoBlobURL: function TVSimGetSimulatorVideoBlob(aTunerId,
                                                                aSourceType,
                                                                aChannelNumber,
                                                                aWin) {
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

  _getTunerMapKey: function TVSimGetTunerMapKey(aTunerId, aSourceType) {
    return JSON.stringify({'tunerId': aTunerId, 'sourceType': aSourceType});
  },

  _getWrapTunerData: function TVSimGetWrapTunerData(aTunerId, aSourceType) {
    if (!this._internalTuners || this._internalTuners.size <= 0) {
      return null;
    }
    return this._internalTuners.get(this._getTunerMapKey(aTunerId, aSourceType));
  },

  _getFilePath: function TVSimGetFilePathFromDummyDirectory(fileName) {
    let dsFile = Cc["@mozilla.org/file/directory_service;1"]
                   .getService(Ci.nsIProperties)
                   .get("ProfD", Ci.nsIFile);
    dsFile.append(TV_SIMULATOR_DUMMY_DIRECTORY);
    dsFile.append(fileName);

    return dsFile.path;
  },

  _validateSettings: function TVSimValidateSettings(aSettingsObject) {
    return this._validateTuners(aSettingsObject.tuners);
  },

  _validateTuners: function TVSimValidateTuners(aTunersObject) {
    let tunerIds = new Array();
    for each (let tuner in aTunersObject) {
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

  _validateSources: function TVSimValidateSources(aSourcesObject) {
    for each (let source in aSourcesObject) {
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

  _validateChannels: function TVSimValidateChannels(aChannelsObject) {
    let channelNumbers = new Array();
    for each (let channel in aChannelsObject) {
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

  _validatePrograms: function TVSimValidatePrograms(aProgramsObject) {
    let eventIds = new Array();
    for each (let program in aProgramsObject) {
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
