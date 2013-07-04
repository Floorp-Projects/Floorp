/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * TestEchoReceiver - receives json data, reserializes it and send it back.
 */ 

var TestEchoReceiver = {
  init: function init() {
    addMessageListener("Test:EchoRequest", this);
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Test:EchoRequest":
        sendAsyncMessage("Test:EchoResponse", json);
        break;
    }
  },

};

TestEchoReceiver.init();

