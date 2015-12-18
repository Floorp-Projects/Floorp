/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

addMessageListener('init-mocked-data', function(aData) {
  var service = Cc["@mozilla.org/tv/tvservice;1"].getService(Ci.nsITVService);
  try {
    service.QueryInterface(Ci.nsITVSimulatorService).initData(aData);
    sendAsyncMessage('init-mocked-data-complete');
  } catch (e) {}
});

addMessageListener('trigger-channel-scanned', function(aData) {
  var service = Cc["@mozilla.org/tv/tvservice;1"].getService(Ci.nsITVService);
  try {
    service.QueryInterface(Ci.nsITVSimulatorService).simulateChannelScanned(aData.tunerId, aData.sourceType);
  } catch (e) {}
});

addMessageListener('trigger-channel-scan-complete', function(aData) {
  var service = Cc["@mozilla.org/tv/tvservice;1"].getService(Ci.nsITVService);
  try {
    service.QueryInterface(Ci.nsITVSimulatorService).simulateChannelScanComplete(aData.tunerId, aData.sourceType);
  } catch (e) {}
});

addMessageListener('trigger-channel-scan-error', function(aData) {
  var service = Cc["@mozilla.org/tv/tvservice;1"].getService(Ci.nsITVService);
  try {
    service.QueryInterface(Ci.nsITVSimulatorService).simulateChannelScanError(aData.tunerId, aData.sourceType);
  } catch (e) {}
});

addMessageListener('trigger-eit-broadcasted', function(aData) {
  var service = Cc["@mozilla.org/tv/tvservice;1"].getService(Ci.nsITVService);
  try {
    service.QueryInterface(Ci.nsITVSimulatorService).simulateEITBroadcasted(aData.tunerId, aData.sourceType, aData.channelNumber);
  } catch (e) {}
});
