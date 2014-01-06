/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;

// use ppmm to handle file-picker message.
let ppmm = Cc['@mozilla.org/parentprocessmessagemanager;1']
             .getService(Ci.nsIMessageListenerManager);

let pickResult = null;

function processPickMessage(message) {
  let sender = message.target.QueryInterface(Ci.nsIMessageSender);
  // reply FilePicker's message
  sender.sendAsyncMessage('file-picked', pickResult);
  // notify caller
  sendAsyncMessage('file-picked-posted', { type: 'file-picked-posted' });
}

function updatePickResult(result) {
  pickResult = result;
  sendAsyncMessage('pick-result-updated', { type: 'pick-result-updated' });
}

ppmm.addMessageListener('file-picker', processPickMessage);
// use update-pick-result to change the expected pick result.
addMessageListener('update-pick-result', updatePickResult);
