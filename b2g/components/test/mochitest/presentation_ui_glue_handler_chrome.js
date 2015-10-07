/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { XPCOMUtils } = Cu.import('resource://gre/modules/XPCOMUtils.jsm');
const { SystemAppProxy } = Cu.import('resource://gre/modules/SystemAppProxy.jsm');

const glue = Cc["@mozilla.org/presentation/requestuiglue;1"]
             .createInstance(Ci.nsIPresentationRequestUIGlue);

SystemAppProxy.addEventListener('mozPresentationChromeEvent', function(aEvent) {
  if (!aEvent.detail || aEvent.detail.type !== 'presentation-launch-receiver') {
    return;
  }
  sendAsyncMessage('presentation-launch-receiver', aEvent.detail);
});

addMessageListener('trigger-ui-glue', function(aData) {
  var promise = glue.sendRequest(aData.url, aData.sessionId);
  promise.then(function(aFrame){
    sendAsyncMessage('iframe-resolved', aFrame);
  });
});

addMessageListener('trigger-presentation-content-event', function(aData) {
  var detail = {
    type: 'presentation-receiver-launched',
    id: aData.sessionId,
    frame: aData.frame
  };
  SystemAppProxy._sendCustomEvent('mozPresentationContentEvent', detail);
});
