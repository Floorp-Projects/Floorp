/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/InterAppCommService.jsm");

let UUIDGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);

const MESSAGE_PORT_ID = UUIDGenerator.generateUUID().toString();
const FAKE_MESSAGE_PORT_ID = UUIDGenerator.generateUUID().toString();
const OUTER_WINDOW_ID = UUIDGenerator.generateUUID().toString();
const REQUEST_ID = UUIDGenerator.generateUUID().toString();

const PUB_APP_MANIFEST_URL = "app://pubApp.gaiamobile.org/manifest.webapp";
const SUB_APP_MANIFEST_URL = "app://subApp.gaiamobile.org/manifest.webapp";

const PUB_APP_PAGE_URL = "app://pubApp.gaiamobile.org/handler.html";
const SUB_APP_PAGE_URL = "app://subApp.gaiamobile.org/handler.html";

const KEYWORD = "test";

function create_message_port_pair(aMessagePortId,
                                  aKeyword,
                                  aPubManifestURL,
                                  aSubManifestURL) {
  InterAppCommService._messagePortPairs[aMessagePortId] = {
    keyword: aKeyword,
    publisher: {
      manifestURL: aPubManifestURL
    },
    subscriber: {
      manifestURL: aSubManifestURL
    }
  };
}

function clear_message_port_pairs() {
  InterAppCommService._messagePortPairs = {};
}

function register_message_port(aMessagePortId,
                               aManifestURL,
                               aPageURL,
                               aTargetSendAsyncMessage) {
  let message = {
    name: "InterAppMessagePort:Register",
    json: {
      messagePortID: aMessagePortId,
      manifestURL: aManifestURL,
      pageURL: aPageURL
    },
    target: {
      sendAsyncMessage: function(aName, aData) {
        if (aTargetSendAsyncMessage) {
          aTargetSendAsyncMessage(aName, aData);
        }
      },
      assertContainApp: function(_manifestURL) {
        return (aManifestURL == _manifestURL);
      }
    }
  };

  InterAppCommService.receiveMessage(message);

  return message.target;
}

function register_message_ports(aMessagePortId,
                                aPubTargetSendAsyncMessage,
                                aSubTargetSendAsyncMessage) {
  let pubTarget = register_message_port(aMessagePortId,
                                        PUB_APP_MANIFEST_URL,
                                        PUB_APP_PAGE_URL,
                                        aPubTargetSendAsyncMessage);

  let subTarget = register_message_port(aMessagePortId,
                                        SUB_APP_MANIFEST_URL,
                                        SUB_APP_PAGE_URL,
                                        aSubTargetSendAsyncMessage);

  return { pubTarget: pubTarget, subTarget: subTarget };
}

function unregister_message_port(aMessagePortId,
                                 aManifestURL) {
  let message = {
    name: "InterAppMessagePort:Unregister",
    json: {
      messagePortID: aMessagePortId,
      manifestURL: aManifestURL
    },
    target: {
      assertContainApp: function(_manifestURL) {
        return (aManifestURL == _manifestURL);
      }
    }
  };

  InterAppCommService.receiveMessage(message);
}

function remove_target(aTarget) {
  let message = {
    name: "child-process-shutdown",
    target: aTarget
  };

  InterAppCommService.receiveMessage(message);
}

function post_message(aMessagePortId,
                      aManifestURL,
                      aMessage) {
  let message = {
    name: "InterAppMessagePort:PostMessage",
    json: {
      messagePortID: aMessagePortId,
      manifestURL: aManifestURL,
      message: aMessage
    },
    target: {
      assertContainApp: function(_manifestURL) {
        return (aManifestURL == _manifestURL);
      }
    }
  };

  InterAppCommService.receiveMessage(message);
}

function create_allowed_connections(aKeyword,
                                    aPubManifestURL,
                                    aSubManifestURL) {
  let allowedPubAppManifestURLs =
    InterAppCommService._allowedConnections[aKeyword] = {};

  allowedPubAppManifestURLs[aPubManifestURL] = [aSubManifestURL];
}

function clear_allowed_connections() {
  InterAppCommService._allowedConnections = {};
}

function get_connections(aManifestURL,
                         aOuterWindowID,
                         aRequestID,
                         aTargetSendAsyncMessage) {
  let message = {
    name: "Webapps:GetConnections",
    json: {
      manifestURL: aManifestURL,
      outerWindowID: aOuterWindowID,
      requestID: aRequestID
    },
    target: {
      sendAsyncMessage: function(aName, aData) {
        if (aTargetSendAsyncMessage) {
          aTargetSendAsyncMessage(aName, aData);
        }
      },
      assertContainApp: function(_manifestURL) {
        return (aManifestURL == _manifestURL);
      }
    }
  };

  InterAppCommService.receiveMessage(message);
}

function cancel_connections(aManifestURL,
                            aKeyword,
                            aPubManifestURL,
                            aSubManifestURL) {
  let message = {
    name: "InterAppConnection:Cancel",
    json: {
      manifestURL: aManifestURL,
      keyword: aKeyword,
      pubAppManifestURL: aPubManifestURL,
      subAppManifestURL: aSubManifestURL
    },
    target: {
      assertContainApp: function(_manifestURL) {
        return (aManifestURL == _manifestURL);
      }
    }
  };

  InterAppCommService.receiveMessage(message);
}

add_test(function test_registerMessagePort() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  let targets = register_message_ports(MESSAGE_PORT_ID);

  let messagePortPair = InterAppCommService._messagePortPairs[MESSAGE_PORT_ID];

  do_check_eq(PUB_APP_PAGE_URL, messagePortPair.publisher.pageURL);
  do_check_eq(SUB_APP_PAGE_URL, messagePortPair.subscriber.pageURL);

  do_check_true(targets.pubTarget === messagePortPair.publisher.target);
  do_check_true(targets.subTarget === messagePortPair.subscriber.target);

  clear_message_port_pairs();
  run_next_test();
});

add_test(function test_failToRegisterMessagePort() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  let targets = register_message_ports(FAKE_MESSAGE_PORT_ID);

  let messagePortPair = InterAppCommService._messagePortPairs[MESSAGE_PORT_ID];

  // Because it failed to register, the page URLs and targets don't exist.
  do_check_true(messagePortPair.publisher.pageURL === undefined);
  do_check_true(messagePortPair.subscriber.pageURL === undefined);

  do_check_true(messagePortPair.publisher.target === undefined);
  do_check_true(messagePortPair.subscriber.target === undefined);

  clear_message_port_pairs();
  run_next_test();
});

add_test(function test_unregisterMessagePort() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  register_message_ports(MESSAGE_PORT_ID);

  unregister_message_port(MESSAGE_PORT_ID, PUB_APP_MANIFEST_URL);

  do_check_true(InterAppCommService._messagePortPairs[MESSAGE_PORT_ID]
                === undefined);

  clear_message_port_pairs();
  run_next_test();
});

add_test(function test_failToUnregisterMessagePort() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  register_message_ports(MESSAGE_PORT_ID);

  unregister_message_port(FAKE_MESSAGE_PORT_ID, PUB_APP_MANIFEST_URL);

  // Because it failed to unregister, the entry still exists.
  do_check_true(InterAppCommService._messagePortPairs[MESSAGE_PORT_ID]
                !== undefined);

  clear_message_port_pairs();
  run_next_test();
});

add_test(function test_removeTarget() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  let targets = register_message_ports(MESSAGE_PORT_ID);

  remove_target(targets.pubTarget);

  do_check_true(InterAppCommService._messagePortPairs[MESSAGE_PORT_ID]
                === undefined);

  clear_message_port_pairs();
  run_next_test();
});

add_test(function test_postMessage() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  let countPubAppOnMessage = 0;
  function pubAppOnMessage(aName, aData) {
    countPubAppOnMessage++;

    do_check_eq(aName, "InterAppMessagePort:OnMessage");
    do_check_eq(aData.manifestURL, PUB_APP_MANIFEST_URL);
    do_check_eq(aData.pageURL, PUB_APP_PAGE_URL);
    do_check_eq(aData.messagePortID, MESSAGE_PORT_ID);

    if (countPubAppOnMessage == 1) {
      do_check_eq(aData.message.text, "sub app says world");

      post_message(MESSAGE_PORT_ID,
                   PUB_APP_MANIFEST_URL,
                   { text: "pub app says hello again" });

    } else if (countPubAppOnMessage == 2) {
      do_check_eq(aData.message.text, "sub app says world again");

      clear_message_port_pairs();
      run_next_test();
    } else {
      do_throw("pub app receives an unexpected message")
    }
  };

  let countSubAppOnMessage = 0;
  function subAppOnMessage(aName, aData) {
    countSubAppOnMessage++;

    do_check_eq(aName, "InterAppMessagePort:OnMessage");
    do_check_eq(aData.manifestURL, SUB_APP_MANIFEST_URL);
    do_check_eq(aData.pageURL, SUB_APP_PAGE_URL);
    do_check_eq(aData.messagePortID, MESSAGE_PORT_ID);

    if (countSubAppOnMessage == 1) {
      do_check_eq(aData.message.text, "pub app says hello");

      post_message(MESSAGE_PORT_ID,
                   SUB_APP_MANIFEST_URL,
                   { text: "sub app says world" });

    } else if (countSubAppOnMessage == 2) {
      do_check_eq(aData.message.text, "pub app says hello again");

      post_message(MESSAGE_PORT_ID,
                   SUB_APP_MANIFEST_URL,
                   { text: "sub app says world again" });
    } else {
      do_throw("sub app receives an unexpected message")
    }
  };

  register_message_ports(MESSAGE_PORT_ID, pubAppOnMessage, subAppOnMessage);

  post_message(MESSAGE_PORT_ID,
               PUB_APP_MANIFEST_URL,
               { text: "pub app says hello" });
});

add_test(function test_registerMessagePort_with_queued_messages() {
  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  register_message_port(MESSAGE_PORT_ID,
                        PUB_APP_MANIFEST_URL,
                        PUB_APP_PAGE_URL);

  post_message(MESSAGE_PORT_ID,
               PUB_APP_MANIFEST_URL,
               { text: "pub app says hello" });

  post_message(MESSAGE_PORT_ID,
               PUB_APP_MANIFEST_URL,
               { text: "pub app says hello again" });

  let countSubAppOnMessage = 0;
  function subAppOnMessage(aName, aData) {
    countSubAppOnMessage++;

    do_check_eq(aName, "InterAppMessagePort:OnMessage");
    do_check_eq(aData.manifestURL, SUB_APP_MANIFEST_URL);
    do_check_eq(aData.pageURL, SUB_APP_PAGE_URL);
    do_check_eq(aData.messagePortID, MESSAGE_PORT_ID);

    if (countSubAppOnMessage == 1) {
      do_check_eq(aData.message.text, "pub app says hello");
    } else if (countSubAppOnMessage == 2) {
      do_check_eq(aData.message.text, "pub app says hello again");

      clear_message_port_pairs();
      run_next_test();
    } else {
      do_throw("sub app receives an unexpected message")
    }
  };

  register_message_port(MESSAGE_PORT_ID,
                        SUB_APP_MANIFEST_URL,
                        SUB_APP_PAGE_URL,
                        subAppOnMessage);
});

add_test(function test_getConnections() {
  create_allowed_connections(KEYWORD,
                             PUB_APP_MANIFEST_URL,
                             SUB_APP_MANIFEST_URL);

  function onGetConnections(aName, aData) {
    do_check_eq(aName, "Webapps:GetConnections:Return:OK");
    do_check_eq(aData.oid, OUTER_WINDOW_ID);
    do_check_eq(aData.requestID, REQUEST_ID);

    let connections = aData.connections;
    do_check_eq(connections.length, 1);
    do_check_eq(connections[0].keyword, KEYWORD);
    do_check_eq(connections[0].pubAppManifestURL, PUB_APP_MANIFEST_URL);
    do_check_eq(connections[0].subAppManifestURL, SUB_APP_MANIFEST_URL);

    clear_allowed_connections();
    run_next_test();
  };

  get_connections(PUB_APP_MANIFEST_URL,
                  OUTER_WINDOW_ID,
                  REQUEST_ID,
                  onGetConnections);
});

add_test(function test_cancelConnection() {
  create_allowed_connections(KEYWORD,
                             PUB_APP_MANIFEST_URL,
                             SUB_APP_MANIFEST_URL);

  create_message_port_pair(MESSAGE_PORT_ID,
                           KEYWORD,
                           PUB_APP_MANIFEST_URL,
                           SUB_APP_MANIFEST_URL);

  register_message_ports(MESSAGE_PORT_ID);

  cancel_connections(PUB_APP_MANIFEST_URL,
                     KEYWORD,
                     PUB_APP_MANIFEST_URL,
                     SUB_APP_MANIFEST_URL);

  do_check_true(InterAppCommService._allowedConnections[KEYWORD]
                === undefined);

  do_check_true(InterAppCommService._messagePortPairs[MESSAGE_PORT_ID]
                === undefined);

  clear_allowed_connections();
  clear_message_port_pairs();
  run_next_test();
});

function run_test() {
  do_get_profile();

  run_next_test();
}
