/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

 function debug(str) {
   dump('presentation_prompt_handler_chrome: ' + str + '\n');
 }

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { XPCOMUtils } = Cu.import('resource://gre/modules/XPCOMUtils.jsm', {});
const { SystemAppProxy } = Cu.import('resource://gre/modules/SystemAppProxy.jsm', {});

const manager = Cc["@mozilla.org/presentation-device/manager;1"]
                  .getService(Ci.nsIPresentationDeviceManager);

const prompt = Cc['@mozilla.org/presentation-device/prompt;1']
                 .getService(Ci.nsIPresentationDevicePrompt);

function TestPresentationDevice(options) {
  this.id = options.id;
  this.name = options.name;
  this.type = options.type;
}

TestPresentationDevice.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
  establishSessionTransport: function() {
    return null;
  },
};

function TestPresentationRequest(options) {
  this.origin = options.origin;
  this.requestURL = options.requestURL;
}

TestPresentationRequest.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceRequest]),
  select: function(device) {
    let result = {
      type: 'select',
      device: {
        id: device.id,
        name: device.name,
        type: device.type,
      },
    };
    sendAsyncMessage('presentation-select-result', result);
  },
  cancel: function() {
    let result = {
      type: 'cancel',
    };
    sendAsyncMessage('presentation-select-result', result);
  },
};

var testDevice = null;

addMessageListener('setup', function(device_options) {
  testDevice = new TestPresentationDevice(device_options);
  manager.QueryInterface(Ci.nsIPresentationDeviceListener).addDevice(testDevice);
  sendAsyncMessage('setup-complete');
});

var eventHandler = function(evt) {
  if (!evt.detail || evt.detail.type !== 'presentation-select-device') {
    return;
  }

  sendAsyncMessage('presentation-select-device', evt.detail);
};

SystemAppProxy.addEventListener('mozChromeEvent', eventHandler);

// need to remove ChromeEvent listener after test finished.
addMessageListener('teardown', function() {
  if (testDevice) {
    manager.removeDevice(testDevice);
  }
  SystemAppProxy.removeEventListener('mozChromeEvent', eventHandler);
});

addMessageListener('trigger-device-prompt', function(request_options) {
  let request = new TestPresentationRequest(request_options);
  prompt.promptDeviceSelection(request);
});

addMessageListener('presentation-select-response', function(detail) {
  SystemAppProxy._sendCustomEvent('mozContentEvent', detail);
});
