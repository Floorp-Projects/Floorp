/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let NFC = {};
subscriptLoader.loadSubScript("resource://gre/components/Nfc.js", NFC);

// Mock nfc service
// loadSubScript could not load const and let variables, type defined
// here should be consistent with type defined in Nfc.js
const MockReqType = {
  CHANGE_RF_STATE: "changeRFState",
  READ_NDEF: "readNDEF",
  WRITE_NDEF: "writeNDEF",
  MAKE_READ_ONLY: "makeReadOnly",
  FORMAT: "format",
  TRANSCEIVE: "transceive"
};

const MockRspType = {};
MockRspType[MockReqType.CHANGE_RF_STATE] = "changeRFStateRsp";
MockRspType[MockReqType.READ_NDEF] = "readNDEFRsp";
MockRspType[MockReqType.WRITE_NDEF] = "writeNDEFRsp";
MockRspType[MockReqType.MAKE_READ_ONLY] = "makeReadOnlyRsp";
MockRspType[MockReqType.FORMAT] = "formatRsp";
MockRspType[MockReqType.TRANSCEIVE] = "transceiveRsp";

const MockNtfType = {
  INITIALIZED: "initialized",
  TECH_DISCOVERED: "techDiscovered",
  TECH_LOST: "techLost",
};

const MOCK_RECORDS = {"payload":{"0":4,"1":119,"2":119,
                                 "3":119,"4":46,"5":109,
                                 "6":111,"7":122,"8":105},
                      "tnf":"well-known",
                      "type":{"0":85}};

let MockNfcService = {
  listener: null,

  setListener: function (listener) {
    this.listener = listener;
  },

  notifyEvent: function (event) {
    if (this.listener && this.listener.onEvent) {
      this.listener.onEvent(event);
    }
  },

  start: function (eventListener) {
    this.setListener(eventListener);
  },

  shutdown: function () {
    this.listener = null;
  },

  sendCommand: function (message) {
    switch (message.type) {
      case MockReqType.CHANGE_RF_STATE:
        this.notifyEvent({ errorMsg: "",
                           requestId: message.requestId,
                           rfState: message.rfState,
                           rspType: MockRspType[message.type] });
        break;
      case MockReqType.READ_NDEF:
        this.notifyEvent({ errorMsg: "",
                           requestId: message.requestId,
                           records: MOCK_RECORDS,
                           sessionId: message.sessionId,
                           rspType: MockRspType[message.type] });
        break;
      case MockReqType.WRITE_NDEF:
      case MockReqType.MAKE_READ_ONLY:
      case MockReqType.TRANSCEIVE:
      case MockReqType.FORMAT:
        this.notifyEvent({ errorMsg: "",
                           requestId: message.requestId,
                           sessionId: message.sessionId,
                           rspType: MockRspType[message.type] });
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  }
};
// end of nfc service mock.

let messageManager = NFC.gMessageManager;
let sessionHelper = NFC.SessionHelper;

new NFC.Nfc(/* isXPCShell */ true);

// It would better to use MockRegistrar but nsINfcService contains
// method marked with implicit_context which may not be implemented in js.
// Use a simple assignment to mock nfcService

MockNfcService.start(messageManager.nfc);
messageManager.nfc.nfcService = MockNfcService;

function run_test() {
  run_next_test();
}

add_test(function test_setFocusTab() {
  let tabId = 1;
  let rsp = "NFC:DOMEvent";
  let focusOnCb = function (message) {
    let result = message.data;

    deepEqual(result,
              { tabId: tabId,
                event: NFC_CONSTS.FOCUS_CHANGED,
                focus: true },
              "Correct result SetFocusTab On");

    let focusOffCb = function (msg) {
      let result = msg.data;

      deepEqual(result,
                { tabId: tabId,
                  event: NFC_CONSTS.FOCUS_CHANGED,
                  focus: false },
                "Correct result SetFocusTab Off");

      equal(messageManager.focusId, NFC_CONSTS.SYSTEM_APP_ID);

      sendSyncMessage("NFC:RemoveEventListener", { tabId: tabId });

      run_next_test();
    };

    sendAsyncMessage("NFC:SetFocusTab", rsp, { tabId: tabId, isFocus: false },
                     focusOffCb);
  };

  sendSyncMessage("NFC:AddEventListener", { tabId: tabId });
  sendAsyncMessage("NFC:SetFocusTab", rsp, { tabId: tabId, isFocus: true },
                   focusOnCb);
});

add_test(function test_checkP2PRegistrationSucceed() {
  let appId = 1;
  let requestId = 10;
  let rsp = "NFC:CheckP2PRegistrationResponse";
  let sessionId = 1;
  let isP2P = true;
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.errorMsg, undefined);

    sendSyncMessage("NFC:UnregisterPeerReadyTarget", { appId: appId });
    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sessionHelper.registerSession(sessionId, isP2P);
  sendSyncMessage("NFC:RegisterPeerReadyTarget", { appId: appId });
  sendAsyncMessage("NFC:CheckP2PRegistration",
                   rsp,
                   { appId: appId,
                     requestId: requestId },
                   callback);
});

add_test(function test_checkP2PRegistrationFailed() {
  let appId = 1;
  let requestId = 10;
  let rsp = "NFC:CheckP2PRegistrationResponse";
  let errorMsg =
    NFC_CONSTS.NFC_ERROR_MSG[NFC_CONSTS.NFC_GECKO_ERROR_P2P_REG_INVALID];
  let callback = function (message) {
    let result = message.data;

    deepEqual(result,
              { requestId: requestId,
                errorMsg: errorMsg },
              "Correct result checkP2PRegistrationFailed");

    run_next_test();
  };

  sendAsyncMessage("NFC:CheckP2PRegistration",
                   rsp,
                   { appId: 10,
                     requestId: requestId },
                   callback);
});

add_test(function test_notifyUserAcceptedP2P() {
  let appId = 1;
  let requestId = 10;
  let rsp = "NFC:DOMEvent";
  let sessionId = 1;
  let isP2P = true;
  let callback = function (message) {
    let result = message.data;

    ok(true, "received CheckP2PRegistrationResponse")

    sendSyncMessage("NFC:UnregisterPeerReadyTarget", { appId: appId });
    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sessionHelper.registerSession(sessionId, isP2P);
  sendSyncMessage("NFC:RegisterPeerReadyTarget", { appId: appId });
  sendAsyncMessage("NFC:NotifyUserAcceptedP2P", rsp, { appId: appId },
                   callback);
});

add_test(function test_changeRFState() {
  let tabId = 1;
  let requestId = 10;
  let msg = "NFC:DOMEvent";
  let rfState = NFC_CONSTS.NFC_RF_STATE_LISTEN;
  let callback = function (message) {
    let result = message.data;

    deepEqual(result,
              { tabId: tabId,
                event: NFC_CONSTS.RF_EVENT_STATE_CHANGED,
                rfState: rfState },
              "Correct result onRFStateChanged");

    sendSyncMessage("NFC:RemoveEventListener", { tabId: tabId });

    run_next_test();
  };

  sendSyncMessage("NFC:AddEventListener", { tabId: tabId });
  sendAsyncMessage("NFC:ChangeRFState",
                   msg,
                   { requestId: requestId,
                     rfState: rfState },
                   callback);
});

add_test(function test_queryRFState() {
  equal(sendSyncMessage("NFC:QueryInfo")[0].rfState, NFC_CONSTS.NFC_RF_STATE_LISTEN);
  run_next_test();
});

add_test(function test_onTagFound() {
  let tabId = 1;
  let msg = "NFC:DOMEvent";
  let sessionId = 10;
  let setFocusCb = function (focusMsg) {
    let callback = function (message) {
      let result = message.data;

      equal(result.tabId, tabId);
      equal(result.event, NFC_CONSTS.TAG_EVENT_FOUND);

      run_next_test();
    };

    waitAsyncMessage(msg, callback);
    MockNfcService.notifyEvent({ ntfType: MockNtfType.TECH_DISCOVERED,
                                 sessionId: sessionId,
                                 isP2P: false });
  };

  sendSyncMessage("NFC:AddEventListener", { tabId: tabId });
  sendAsyncMessage("NFC:SetFocusTab",
                   msg,
                   { tabId: tabId,
                     isFocus: true },
                   setFocusCb);
});

add_test(function test_onTagLost() {
  let tabId = 1;
  let msg = "NFC:DOMEvent";
  let sessionId = 10;
  let callback = function (message) {
    let result = message.data;

    equal(result.tabId, tabId);
    equal(result.event, NFC_CONSTS.TAG_EVENT_LOST);

    sendSyncMessage("NFC:RemoveEventListener", { tabId: tabId });
    sessionHelper.tokenMap = {};
    sendSyncMessage("NFC:SetFocusTab",
                     { tabId: tabId,
                       isFocus: false });

    run_next_test();
  };

  waitAsyncMessage(msg, callback);
  MockNfcService.notifyEvent({ ntfType: MockNtfType.TECH_LOST,
                               sessionId: sessionId,
                               isP2P: false });
});

add_test(function test_onPeerFound() {
  let tabId = 1;
  let msg = "NFC:DOMEvent";
  let sessionId = 10;
  let setFocusCb = function (focusMsg) {
    let callback = function (message) {
      let result = message.data;

      equal(result.tabId, tabId);
      equal(result.event, NFC_CONSTS.PEER_EVENT_FOUND);

      run_next_test();
    };

    waitAsyncMessage(msg, callback);
    MockNfcService.notifyEvent({ ntfType: MockNtfType.TECH_DISCOVERED,
                                 sessionId: sessionId,
                                 isP2P: true });
  };

  sendSyncMessage("NFC:AddEventListener", { tabId: tabId });
  sendAsyncMessage("NFC:SetFocusTab",
                   msg,
                   { tabId: tabId,
                     isFocus: true },
                   setFocusCb);

});

add_test(function test_onPeerLost() {
  let tabId = 1;
  let msg = "NFC:DOMEvent";
  let sessionId = 10;
  let callback = function (message) {
    let result = message.data;

    equal(result.tabId, tabId);
    equal(result.event, NFC_CONSTS.PEER_EVENT_LOST);

    sendSyncMessage("NFC:RemoveEventListener", { tabId: tabId });
    sessionHelper.tokenMap = {};
    sendSyncMessage("NFC:SetFocusTab",
                     { tabId: tabId,
                       isFocus: false });

    run_next_test();
  };

  waitAsyncMessage(msg, callback);
  MockNfcService.notifyEvent({ ntfType: MockNtfType.TECH_LOST,
                               sessionId: sessionId,
                               isP2P: true });

});

add_test(function test_readNDEF() {
  let requestId = 10;
  let sessionId = 15;
  let msg = "NFC:ReadNDEFResponse";
  let sessionToken = sessionHelper.registerSession(sessionId, false);
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.sessionId, sessionId);
    deepEqual(result.records, MOCK_RECORDS);

    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sendAsyncMessage("NFC:ReadNDEF",
                   msg,
                   { requestId: requestId,
                     sessionToken: sessionToken },
                   callback);
});

add_test(function test_writeNDEF() {
  let requestId = 10;
  let sessionId = 15;
  let msg = "NFC:WriteNDEFResponse";
  let sessionToken = sessionHelper.registerSession(sessionId, false);
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.sessionId, sessionId);

    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sendAsyncMessage("NFC:WriteNDEF",
                   msg,
                   { requestId: requestId,
                     sessionToken: sessionToken,
                     records: MOCK_RECORDS,
                     isP2P: true },
                   callback);
});

add_test(function test_format() {
  let requestId = 10;
  let sessionId = 15;
  let msg = "NFC:FormatResponse";
  let sessionToken = sessionHelper.registerSession(sessionId, false);
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.sessionId, sessionId);

    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sendAsyncMessage("NFC:Format",
                   msg,
                   { requestId: requestId,
                     sessionToken: sessionToken },
                   callback);
});

add_test(function test_makeReadOnly() {
  let requestId = 10;
  let sessionId = 15;
  let msg = "NFC:MakeReadOnlyResponse";
  let sessionToken = sessionHelper.registerSession(sessionId, false);
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.sessionId, sessionId);

    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sendAsyncMessage("NFC:MakeReadOnly",
                   msg,
                   { requestId: requestId,
                     sessionToken: sessionToken },
                   callback);
});


add_test(function test_transceive() {
  let requestId = 10;
  let sessionId = 15;
  let msg = "NFC:TransceiveResponse";
  let sessionToken = sessionHelper.registerSession(sessionId, false);
  let callback = function (message) {
    let result = message.data;

    equal(result.requestId, requestId);
    equal(result.sessionId, sessionId);

    sessionHelper.tokenMap = {};

    run_next_test();
  };

  sendAsyncMessage("NFC:Transceive",
                   msg,
                   { requestId: requestId,
                     sessionToken: sessionToken,
                     technology: "NFCA",
                     command: "0x50" },
                   callback);
});
