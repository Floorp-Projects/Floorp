/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let MMDB = {};
Cu.import("resource://gre/modules/MobileMessageDB.jsm", MMDB);

const RIL_MOBILEMESSAGEDATABASESERVICE_CONTRACTID =
  "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1";
const RIL_MOBILEMESSAGEDATABASESERVICE_CID =
  Components.ID("{29785f90-6b5b-11e2-9201-3b280170b2ec}");

XPCOMUtils.defineLazyServiceGetter(this, "gDiskSpaceWatcher",
                                   "@mozilla.org/toolkit/disk-space-watcher;1",
                                   "nsIDiskSpaceWatcher");

const DB_NAME = "sms";

/**
 * MobileMessageDatabaseService
 */
function MobileMessageDatabaseService() {
  // Prime the directory service's cache to ensure that the ProfD entry exists
  // by the time IndexedDB queries for it off the main thread. (See bug 743635.)
  Services.dirsvc.get("ProfD", Ci.nsIFile);

  let mmdb = new MMDB.MobileMessageDB();
  mmdb.init(DB_NAME, 0, mmdb.updatePendingTransactionToError.bind(mmdb));
  mmdb.isDiskFull = function() {
    return gDiskSpaceWatcher.isDiskFull;
  };
  this.mmdb = mmdb;
}
MobileMessageDatabaseService.prototype = {

  classID: RIL_MOBILEMESSAGEDATABASESERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRilMobileMessageDatabaseService,
                                         Ci.nsIMobileMessageDatabaseService,
                                         Ci.nsIObserver]),

  /**
   * MobileMessageDB instance.
   */
  mmdb: null,

  /**
   * nsIObserver
   */
  observe: function() {},

  /**
   * nsIRilMobileMessageDatabaseService API
   */

  saveReceivedMessage: function(aMessage, aCallback) {
    this.mmdb.saveReceivedMessage(aMessage, aCallback);
  },

  saveSendingMessage: function(aMessage, aCallback) {
    this.mmdb.saveSendingMessage(aMessage, aCallback);
  },

  setMessageDeliveryByMessageId: function(aMessageId, aReceiver, aDelivery,
                                          aDeliveryStatus, aEnvelopeId,
                                          aCallback) {
    this.mmdb.updateMessageDeliveryById(aMessageId, "messageId", aReceiver,
                                        aDelivery, aDeliveryStatus,
                                        aEnvelopeId, aCallback);
  },

  setMessageDeliveryStatusByEnvelopeId: function(aEnvelopeId, aReceiver,
                                                 aDeliveryStatus, aCallback) {
    this.mmdb.updateMessageDeliveryById(aEnvelopeId, "envelopeId", aReceiver,
                                        null, aDeliveryStatus, null, aCallback);
  },

  setMessageReadStatusByEnvelopeId: function(aEnvelopeId, aReceiver,
                                             aReadStatus, aCallback) {
    this.mmdb.setMessageReadStatusByEnvelopeId(aEnvelopeId, aReceiver,
                                               aReadStatus, aCallback);
  },

  getMessageRecordByTransactionId: function(aTransactionId, aCallback) {
    this.mmdb.getMessageRecordByTransactionId(aTransactionId, aCallback);
  },

  getMessageRecordById: function(aMessageId, aCallback) {
    this.mmdb.getMessageRecordById(aMessageId, aCallback);
  },

  translateCrErrorToMessageCallbackError: function(aCrError) {
    return this.mmdb.translateCrErrorToMessageCallbackError(aCrError);
  },

  /**
   * nsIMobileMessageDatabaseService API
   */

  getMessage: function(aMessageId, aRequest) {
    this.mmdb.getMessage(aMessageId, aRequest);
  },

  deleteMessage: function(aMessageIds, aLength, aRequest) {
    this.mmdb.deleteMessage(aMessageIds, aLength, aRequest);
  },

  createMessageCursor: function(aFilter, aReverse, aCallback) {
    return this.mmdb.createMessageCursor(aFilter, aReverse, aCallback);
  },

  markMessageRead: function(aMessageId, aValue, aSendReadReport, aRequest) {
    this.mmdb.markMessageRead(aMessageId, aValue, aSendReadReport, aRequest);
  },

  createThreadCursor: function(aCallback) {
    return this.mmdb.createThreadCursor(aCallback);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MobileMessageDatabaseService]);
