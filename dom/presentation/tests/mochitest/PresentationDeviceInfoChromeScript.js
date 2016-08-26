/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/PresentationDeviceInfoManager.jsm');

const { XPCOMUtils } = Cu.import('resource://gre/modules/XPCOMUtils.jsm');

const manager = Cc['@mozilla.org/presentation-device/manager;1']
                  .getService(Ci.nsIPresentationDeviceManager);

var testProvider = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceProvider]),
  forceDiscovery: function() {
    sendAsyncMessage('force-discovery');
  },
  listener: null,
};

var testDevice = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  establishControlChannel: function() {
    return null;
  },
  disconnect: function() {},
  isRequestedUrlSupported: function(requestedUrl) {
    return true;
  },
  id: null,
  name: null,
  type: null,
  listener: null,
};

addMessageListener('setup', function() {
  manager.addDeviceProvider(testProvider);

  sendAsyncMessage('setup-complete');
});

addMessageListener('trigger-device-add', function(device) {
  testDevice.id = device.id;
  testDevice.name = device.name;
  testDevice.type = device.type;
  manager.addDevice(testDevice);
});

addMessageListener('trigger-device-update', function(device) {
  testDevice.id = device.id;
  testDevice.name = device.name;
  testDevice.type = device.type;
  manager.updateDevice(testDevice);
});

addMessageListener('trigger-device-remove', function() {
  manager.removeDevice(testDevice);
});

addMessageListener('teardown', function() {
  manager.removeDeviceProvider(testProvider);
});
