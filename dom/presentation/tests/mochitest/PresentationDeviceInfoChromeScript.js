/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

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

var testDevice1 = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id: 'dummyid',
  name: 'dummyName',
  type: 'dummyType',
  establishControlChannel: function(url, presentationId) {
    return null;
  },
  disconnect: function() {},
  isRequestedUrlSupported: function(requestedUrl) {
    return true;
  },
};

var testDevice2 = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id: 'dummyid',
  name: 'dummyName',
  type: 'dummyType',
  establishControlChannel: function(url, presentationId) {
    return null;
  },
  disconnect: function() {},
  isRequestedUrlSupported: function(requestedUrl) {
    return true;
  },
};

var mockedDeviceWithoutSupportedURL = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id: 'dummyid',
  name: 'dummyName',
  type: 'dummyType',
  establishControlChannel: function(url, presentationId) {
    return null;
  },
  disconnect: function() {},
  isRequestedUrlSupported: function(requestedUrl) {
    return false;
  },
};

var mockedDeviceSupportHttpsURL = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  id: 'dummyid',
  name: 'dummyName',
  type: 'dummyType',
  establishControlChannel: function(url, presentationId) {
    return null;
  },
  disconnect: function() {},
  isRequestedUrlSupported: function(requestedUrl) {
    if (requestedUrl.indexOf("https://") != -1) {
      return true;
    }
    return false;
  },
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

addMessageListener('trigger-add-unsupport-url-device', function() {
  manager.addDevice(mockedDeviceWithoutSupportedURL);
});

addMessageListener('trigger-add-multiple-devices', function() {
  manager.addDevice(testDevice1);
  manager.addDevice(testDevice2);
});

addMessageListener('trigger-add-https-devices', function() {
  manager.addDevice(mockedDeviceSupportHttpsURL);
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

addMessageListener('trigger-remove-unsupported-device', function() {
  manager.removeDevice(mockedDeviceWithoutSupportedURL);
});

addMessageListener('trigger-remove-multiple-devices', function() {
  manager.removeDevice(testDevice1);
  manager.removeDevice(testDevice2);
});

addMessageListener('trigger-remove-https-devices', function() {
  manager.removeDevice(mockedDeviceSupportHttpsURL);
});

addMessageListener('teardown', function() {
  manager.removeDeviceProvider(testProvider);
});
