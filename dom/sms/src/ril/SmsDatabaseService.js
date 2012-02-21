/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const RIL_SMSDATABASESERVICE_CONTRACTID = "@mozilla.org/sms/rilsmsdatabaseservice;1";
const RIL_SMSDATABASESERVICE_CID = Components.ID("{a1fa610c-eb6c-4ac2-878f-b005d5e89249}");

/**
 * SmsDatabaseService
 */
function SmsDatabaseService() {
}
SmsDatabaseService.prototype = {

  classID:   RIL_SMSDATABASESERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISmsDatabaseService]),

  // nsISmsDatabaseService

  saveSentMessage: function saveSentMessage(receiver, body, date) {
    return -1;
  },

  getMessage: function getMessage(messageId, requestId) {
  },

  deleteMessage: function deleteMessage(messageId, requestId) {
  },

  createMessageList: function createMessageList(filter, reverse, requestId) {
  },

  getNextMessageInList: function getNextMessageInList(listId, requestId) {
  },

  clearMessageList: function clearMessageList(listId) {
  }

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SmsDatabaseService]);
