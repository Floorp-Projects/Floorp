/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const DEBUG = false; // set to true to see debug messages

const RILCONTENTHELPER_CID =
  Components.ID("{472816e1-1fd6-4405-996c-806f9ea68174}");
const MOBILECONNECTIONINFO_CID =
  Components.ID("{a35cfd39-2d93-4489-ac7d-396475dacb27}");

const RIL_IPC_MSG_NAMES = [
  "RIL:CardStateChanged",
  "RIL:VoiceInfoChanged",
  "RIL:DataInfoChanged",
];

const kVoiceChangedTopic     = "mobile-connection-voice-changed";
const kDataChangedTopic      = "mobile-connection-data-changed";
const kCardStateChangedTopic = "mobile-connection-cardstate-changed";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIFrameMessageManager");

function MobileConnectionInfo() {}
MobileConnectionInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMMozMobileConnectionInfo]),
  classID:        MOBILECONNECTIONINFO_CID,
  classInfo:      XPCOMUtils.generateCI({
    classID:          MOBILECONNECTIONINFO_CID,
    classDescription: "MobileConnectionInfo",
    flags:            Ci.nsIClassInfo.DOM_OBJECT,
    interfaces:       [Ci.nsIDOMMozMobileConnectionInfo]
  }),

  // nsIDOMMozMobileConnectionInfo

  connected: false,
  emergencyCallsOnly: false,
  roaming: false,
  operator: null,
  type: null,
  signalStrength: null,
  relSignalStrength: null
};


function RILContentHelper() {
  this.voiceConnectionInfo = new MobileConnectionInfo();
  this.dataConnectionInfo = new MobileConnectionInfo();

  for each (let msgname in RIL_IPC_MSG_NAMES) {
    cpmm.addMessageListener(msgname, this);
  }
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  // Request initial state.
  let radioState = cpmm.QueryInterface(Ci.nsISyncMessageSender)
                       .sendSyncMessage("RIL:GetRadioState")[0];
  if (!radioState) {
    debug("Received null radioState from chrome process.");
    return;
  }
  this.cardState = radioState.cardState;
  for (let key in radioState.voice) {
    this.voiceConnectionInfo[key] = radioState.voice[key];
  }
  for (let key in radioState.data) {
    this.dataConnectionInfo[key] = radioState.data[key];
  }
}
RILContentHelper.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileConnectionProvider,
                                         Ci.nsIRILContentHelper,
                                         Ci.nsIObserver]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIMobileConnectionProvider,
                                                 Ci.nsIRILContentHelper]}),

  // nsIRILContentHelper

  cardState:           RIL.GECKO_CARDSTATE_UNAVAILABLE,
  voiceConnectionInfo: null,
  dataConnectionInfo:  null,

  getNetworks: function getNetworks(window) {
    //TODO bug 744344
    throw Components.Exception("Not implemented", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      for each (let msgname in RIL_IPC_MSG_NAMES) {
        cpmm.removeMessageListener(msgname, this);
      }
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    }
  },

  // nsIFrameMessageListener

  receiveMessage: function receiveMessage(msg) {
    debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.cardState != msg.json.cardState) {
          this.cardState = msg.json.cardState;
          Services.obs.notifyObservers(null, kCardStateChangedTopic, null);
        }
        break;
      case "RIL:VoiceInfoChanged":
        for (let key in msg.json) {
          this.voiceConnectionInfo[key] = msg.json[key];
        }
        Services.obs.notifyObservers(null, kVoiceChangedTopic, null);
        break;
      case "RIL:DataInfoChanged":
        for (let key in msg.json) {
          this.dataConnectionInfo[key] = msg.json[key];
        }
        Services.obs.notifyObservers(null, kDataChangedTopic, null);
        break;
    }
  },

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RILContentHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
