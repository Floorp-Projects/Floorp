/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/InterAppCommService.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

let UUIDGenerator = Cc["@mozilla.org/uuid-generator;1"]
                      .getService(Ci.nsIUUIDGenerator);

const MESSAGE_PORT_ID = UUIDGenerator.generateUUID().toString();
const FAKE_MESSAGE_PORT_ID = UUIDGenerator.generateUUID().toString();
const OUTER_WINDOW_ID = UUIDGenerator.generateUUID().toString();
const TOP_WINDOW_ID = UUIDGenerator.generateUUID().toString();
const REQUEST_ID = UUIDGenerator.generateUUID().toString();

const PUB_APP_MANIFEST_URL = "app://pubapp.gaiamobile.org/manifest.webapp";
const PUB_APP_MANIFEST_URL_WRONG =
                  "app://pubappnotaccepted.gaiamobile.org/manifest.webapp";
const SUB_APP_MANIFEST_URL = "app://subapp.gaiamobile.org/manifest.webapp";
const SUB_APP_MANIFEST_URL_WRONG =
                  "app://subappnotaccepted.gaiamobile.org/manifest.webapp";

const PUB_APP_PAGE_URL = "app://pubapp.gaiamobile.org/handler.html";
const PUB_APP_PAGE_URL_WRONG = "app://pubapp.gaiamobile.org/notAccepted.html";
const SUB_APP_PAGE_URL = "app://subapp.gaiamobile.org/handler.html";
const SUB_APP_PAGE_URL_WORNG = "app://subapp.gaiamobile.org/notAccepted.html";

const PAGE_URL_REG_EXP = "^app://.*\\.gaiamobile\\.org/handler.html";

const KEYWORD = "test";
const CONNECT_KEYWORD = "connect-test";

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
      do_throw("sub app receives an unexpected message");
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
      do_throw("sub app receives an unexpected message");
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


function registerConnection(aKeyword, aSubAppPageURL, aSubAppManifestURL,
                            aDescription, aRules) {
  var subAppPageUrl = Services.io.newURI(aSubAppPageURL, null, null);
  var subAppManifestUrl = Services.io.newURI(aSubAppManifestURL, null, null);
  InterAppCommService.registerConnection(aKeyword, subAppPageUrl,
                                         subAppManifestUrl, aDescription,
                                         aRules);
}

add_test(function test_registerConnection() {
  InterAppCommService._registeredConnections = {};
  var description = "A test connection";

  // Rules can have (ATM):
  //  * minimumAccessLevel
  //  * manifestURLs
  //  * pageURLs
  //  * installOrigins
  var sampleRules = {
    minimumAccessLevel: "certified",
    pageURLs: ["http://a.server.com/a/page.html"]
  };

  registerConnection(CONNECT_KEYWORD, SUB_APP_PAGE_URL, SUB_APP_MANIFEST_URL,
                     description, sampleRules);

  var regConn = InterAppCommService._registeredConnections[CONNECT_KEYWORD];
  do_check_true(regConn !== undefined);
  var regEntry = regConn[SUB_APP_MANIFEST_URL];
  do_check_true(regEntry !== undefined);
  do_check_eq(regEntry.pageURL, SUB_APP_PAGE_URL);
  do_check_eq(regEntry.description, description);
  do_check_eq(regEntry.rules, sampleRules);
  do_check_eq(regEntry.manifestURL, SUB_APP_MANIFEST_URL);

  InterAppCommService._registeredConnections = {};

  run_next_test();
});


// Simulates mozApps connect
function connect(publisher, aTargetSendAsyncMessage) {
  let message = {
    name: "Webapps:Connect",
    json: {
      keyword: publisher.connectKw,
      rules: publisher.rules,
      manifestURL: publisher.manifestURL,
      pubPageURL: publisher.pageURL,
      outerWindowID: OUTER_WINDOW_ID,
      topWindowID: TOP_WINDOW_ID,
      requestID: REQUEST_ID
    },
    target: {
      sendAsyncMessage: function(aName, aData) {
        if (aTargetSendAsyncMessage) {
          aTargetSendAsyncMessage(aName, aData);
        }
      },
      assertContainApp: function(_manifestURL) {
        return (publisher.manifestURL == _manifestURL);
      }
    }
  };
  InterAppCommService.receiveMessage(message);
};

function registerComponent(aObject, aDescription, aContract) {
  var uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                        .getService(Ci.nsIUUIDGenerator);
  var cid = uuidGenerator.generateUUID();

  var componentManager =
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.registerFactory(cid, aDescription, aContract, aObject);

   // Keep the id on the object so we can unregister later.
   aObject.cid = cid;
}

function unregisterComponent(aObject) {
  var componentManager =
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.unregisterFactory(aObject.cid, aObject);
}

// The fun thing about this mock is that it actually does the same than the
// current actual implementation does.
var mockUIGlue =  {
   // nsISupports implementation.
   QueryInterface: function(iid) {
     if (iid.equals(Ci.nsISupports) ||
         iid.equals(Ci.nsIFactory) ||
         iid.equals(Ci.nsIInterAppCommUIGlue)) {
       return this;
     }

     throw Cr.NS_ERROR_NO_INTERFACE;
   },

   // nsIFactory implementation.
   createInstance: function(outer, iid) {
    return this.QueryInterface(iid);
   },

   // nsIActivityUIGlue implementation.
   selectApps: function(aCallerID, aPubAppManifestURL, aKeyword,
                        aAppsToSelect) {
     return Promise.resolve({
       callerID: aCallerID,
       keyword: aKeyword,
       manifestURL: aPubAppManifestURL,
       selectedApps: aAppsToSelect
     });
   }
 };

// Used to keep a fake table of installed apps (needed by mockAppsService)
var webappsTable = {
};

var mockAppsService = {
  // We use the status and the installOrigin here only.
  getAppByManifestURL: function (aPubAppManifestURL) {
    return webappsTable[aPubAppManifestURL];
  },

  // And so far so well this should not be used by tests at all.
  getManifestURLByLocalId: function(appId) {
  }
};


// Template test for connect
function simpleConnectTestTemplate(aTestCase) {
  dump("TEST: " + aTestCase.subscriber.description + "\n");

  // First, add some mocking....

  // System Messenger
  var resolveMessenger, rejectMessenger;
  var messengerPromise = new Promise((resolve, reject) => {
    resolveMessenger = resolve;
    rejectMessenger = reject;
  });
  var mockMessenger = {
    sendMessage: function(aName, aData, aDestURL, aDestManURL) {
      do_check_eq(aName, "connection");
      resolveMessenger(aData);
    }
  };
  InterAppCommService.messenger = mockMessenger;

  // AppsService
  InterAppCommService.appsService = mockAppsService;

  // Set the initial state:
  // First, setup our fake webappsTable:
  var subs = aTestCase.subscriber;
  var pub = aTestCase.publisher;
  webappsTable[subs.manifestURL] = subs.webappEntry;
  webappsTable[pub.manifestURL] = pub.webappEntry;

  InterAppCommService._registeredConnections = {};
  clear_allowed_connections();
  clear_message_port_pairs();

  registerConnection(subs.connectKw, subs.pageURL, subs.manifestURL,
                     subs.description, subs.rules);

  // And now we can try connecting...
  connect(pub, function(aName, aData) {

    var expectedName = "Webapps:Connect:Return:OK";
    var expectedLength = 1;
    if (!aTestCase.expectedSuccess) {
      expectedName = "Webapps:Connect:Return:KO";
      expectedLength = 0;
    }

    do_check_eq(aName, expectedName);
    var numPortIDs =
      (aData.messagePortIDs && aData.messagePortIDs.length ) || 0;
    do_check_eq(numPortIDs, expectedLength);
    if (expectedLength) {
      var portPair =
        InterAppCommService._messagePortPairs[aData.messagePortIDs[0]];
      do_check_eq(portPair.publisher.manifestURL,pub.manifestURL);
      do_check_eq(portPair.subscriber.manifestURL, subs.manifestURL);
    } else {
      run_next_test();
      return;
    }

    // We need to wait for the message to be "received" on the publisher also
    messengerPromise.then(messageData => {
      do_check_eq(messageData.keyword, subs.connectKw);
      do_check_eq(messageData.pubPageURL, pub.pageURL);
      do_check_eq(messageData.messagePortID, aData.messagePortIDs[0]);
      // Cleanup
      InterAppCommService.registeredConnections = {};
      clear_allowed_connections();
      clear_message_port_pairs();
      run_next_test();
    });

  });
}

const CERTIFIED = Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
const PRIVILEGED = Ci.nsIPrincipal.APP_STATUS_PRIVILEGED;
const INSTALLED = Ci.nsIPrincipal.APP_STATUS_INSTALLED;
const NOT_INSTALLED = Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED;

var connectTestCases = [
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "Trivial case [empty rules]. Successful test",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "not certified SUB status and not PUB rules",
      rules: {},
      webappEntry: {
        appStatus: PRIVILEGED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "not certified PUB status and not SUB rules",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: INSTALLED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchMinimumAccessLvl --> Sub INSTALLED PubRul web",
      rules: {},
      webappEntry: {
        appStatus: INSTALLED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        minimumAccessLevel:"web"
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchMinimumAccessLvl --> Sub NOT INSTALLED PubRul web",
      rules: {},
      webappEntry: {
        appStatus: NOT_INSTALLED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        minimumAccessLevel:"web"
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchMinimumAccessLvl --> Pub CERTIFIED SubRul certified",
      rules: {
        minimumAccessLevel:"certified"
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchMinimumAccessLvl --> Pub PRIVILEGED SubRul certified",
      rules: {
        minimumAccessLevel:"certified"
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: PRIVILEGED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchManifest --> Pub manifest1 SubRules:{ manifest1 }",
      rules: {
        manifestURLs: [PUB_APP_MANIFEST_URL]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchManifest --> Pub manifest2 SubRules:{ manifest1 }",
      rules: {
        manifestURLs: [PUB_APP_MANIFEST_URL]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL_WRONG,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchManifest --> Sub manifest1 PubRules:{ manifest1 }",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        manifestURLs: [SUB_APP_MANIFEST_URL]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL_WRONG,
      description: "matchManifest --> Sub manifest2 PubRules:{ manifest1 }",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        manifestURLs: [SUB_APP_MANIFEST_URL]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false

  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL_WRONG,
      description: "matchPage --> Pub page1 SubRules:{ page1 }",
      rules: {
        pageURLs: [PAGE_URL_REG_EXP]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchPage --> Pub page2 SubRules:{ page1 }",
      rules: {
        pageURLs: [PAGE_URL_REG_EXP]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL_WRONG,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchPage --> Sub page1 PubRules:{ page1 }",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        pageURLs: [PAGE_URL_REG_EXP]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: true
  },
  {
    subscriber: {
      connectKw: CONNECT_KEYWORD,
      pageURL: SUB_APP_PAGE_URL_WORNG,
      manifestURL: SUB_APP_MANIFEST_URL,
      description: "matchPage --> Sub page2 PubRules:{ page1 }",
      rules: {},
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    publisher: {
      connectKw: CONNECT_KEYWORD,
      pageURL: PUB_APP_PAGE_URL,
      manifestURL: PUB_APP_MANIFEST_URL,
      rules: {
        pageURLs: [PAGE_URL_REG_EXP]
      },
      webappEntry: {
        appStatus: CERTIFIED,
        installOrigin: "app://system.gaiamobile.org"
      }
    },
    expectedSuccess: false
  }
];

// Only run these test cases if we're on a nightly build. Otherwise they
// don't make much sense.
if (AppConstants.NIGHTLY_BUILD) {
  registerComponent(mockUIGlue,
                    "Mock InterApp UI Glue",
                    "@mozilla.org/dom/apps/inter-app-comm-ui-glue;1");

  do_register_cleanup(function () {
    // Cleanup the mocks
    InterAppCommService.messenger = undefined;
    InterAppCommService.appsService = undefined;
    unregisterComponent(mockUIGlue);
  });
  connectTestCases.forEach(
    aTestCase =>
      add_test(simpleConnectTestTemplate.bind(undefined, aTestCase))
  );
}

function run_test() {
  do_get_profile();

  run_next_test();
}
