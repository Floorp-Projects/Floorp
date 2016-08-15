/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function loadPrivilegedScriptTest() {
  /**
   * The script is loaded as
   * (a) a privileged script in content process for dc_sender.html
   * (b) a frame script in the remote iframe process for dc_receiver_oop.html
   * |type port == "undefined"| indicates the script is load by
   * |loadPrivilegedScript| which is the first case.
   */
  function sendMessage(type, data) {
    if (typeof port == "undefined") {
      sendAsyncMessage(type, {'data': data});
    } else {
      port.postMessage({'type': type,
                        'data': data
                       });
    }
  }

  if (typeof port != "undefined") {
    /**
     * When the script is loaded by |loadPrivilegedScript|, these APIs
     * are exposed to this script.
     */
    port.onmessage = (e) => {
      var type = e.data['type'];
      if (!handlers.hasOwnProperty(type)) {
        return;
      }
      var args = [e];
      handlers[type].forEach(handler => handler.apply(null, args));
    };
    var handlers = {};
    addMessageListener = function(message, handler) {
      if (handlers.hasOwnProperty(message)) {
        handlers[message].push(handler);
      } else {
        handlers[message] = [handler];
      }
    };
    removeMessageListener = function(message, handler) {
      if (!handler || !handlers.hasOwnProperty(message)) {
        return;
      }
      var index = handlers[message].indexOf(handler);
      if (index != -1) {
        handlers[message].splice(index, 1);
      }
    };
  }

  const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

  const mockedChannelDescription = {
    QueryInterface : function (iid) {
      const interfaces = [Ci.nsIPresentationChannelDescription];

      if (!interfaces.some(v => iid.equals(v))) {
          throw Cr.NS_ERROR_NO_INTERFACE;
      }
      return this;
    },
    get type() {
      if (Services.prefs.getBoolPref("dom.presentation.session_transport.data_channel.enable")) {
        return Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL;
      }
      return Ci.nsIPresentationChannelDescription.TYPE_TCP;
    },
    get dataChannelSDP() {
      return "test-sdp";
    }
  };

  function setTimeout(callback, delay) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback({ notify: callback },
                           delay,
                           Ci.nsITimer.TYPE_ONE_SHOT);
    return timer;
  }

  const mockedSessionTransport = {
    QueryInterface : function (iid) {
        const interfaces = [Ci.nsIPresentationSessionTransport,
                            Ci.nsIPresentationDataChannelSessionTransportBuilder,
                            Ci.nsIFactory];

        if (!interfaces.some(v => iid.equals(v))) {
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },
    createInstance: function(aOuter, aIID) {
      if (aOuter) {
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return this.QueryInterface(aIID);
    },
    set callback(callback) {
      this._callback = callback;
    },
    get callback() {
      return this._callback;
    },
    /* OOP case */
    buildDataChannelTransport: function(role, window, listener) {
      dump("PresentationSessionFrameScript: build data channel transport\n");
      this._listener = listener;
      this._role = role;

      var hasNavigator = window ? (typeof window.navigator != "undefined") : false;
      sendMessage('check-navigator', hasNavigator);

      if (this._role == Ci.nsIPresentationService.ROLE_CONTROLLER) {
        this._listener.sendOffer(mockedChannelDescription);
      }
    },

    enableDataNotification: function() {
      sendMessage('data-transport-notification-enabled');
    },
    send: function(data) {
      sendMessage('message-sent', data);
    },
    close: function(reason) {
      sendMessage('data-transport-closed', reason);
      this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportClosed(reason);
      this._callback = null;
    },
    simulateTransportReady: function() {
      this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyTransportReady();
    },
    simulateIncomingMessage: function(message) {
      this._callback.QueryInterface(Ci.nsIPresentationSessionTransportCallback).notifyData(message);
    },
    onOffer: function(aOffer) {
      this._listener.sendAnswer(mockedChannelDescription);
      this._onSessionTransport();
    },
    onAnswer: function(aAnswer) {
      this._onSessionTransport();
    },
    _onSessionTransport: function() {
      setTimeout(()=>{
        this._listener.onSessionTransport(this);
        this.simulateTransportReady();
        this._listener = null;
      }, 0);
    }
  };


  function tearDown() {
    mockedSessionTransport.callback = null;

    /* Register original factories. */
    for (var data of originalFactoryData) {
      registerOriginalFactory(data.contractId, data.mockedClassId,
                              data.mockedFactory, data.originalClassId,
                              data.originalFactory);
    }
    sendMessage("teardown-complete");
  }


  function registerMockedFactory(contractId, mockedClassId, mockedFactory) {
    var originalClassId, originalFactory;
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

    if (!registrar.isCIDRegistered(mockedClassId)) {
      try {
        originalClassId = registrar.contractIDToCID(contractId);
        originalFactory = Cm.getClassObject(Cc[contractId], Ci.nsIFactory);
      } catch (ex) {
        originalClassId = "";
        originalFactory = null;
      }
      if (originalFactory) {
        registrar.unregisterFactory(originalClassId, originalFactory);
      }
      registrar.registerFactory(mockedClassId, "", contractId, mockedFactory);
    }

    return { contractId: contractId,
             mockedClassId: mockedClassId,
             mockedFactory: mockedFactory,
             originalClassId: originalClassId,
             originalFactory: originalFactory };
  }

  function registerOriginalFactory(contractId, mockedClassId, mockedFactory, originalClassId, originalFactory) {
    if (originalFactory) {
      var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
      registrar.unregisterFactory(mockedClassId, mockedFactory);
      registrar.registerFactory(originalClassId, "", contractId, originalFactory);
    }
  }

  /* Register mocked factories. */
  const originalFactoryData = [];
  const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                        .getService(Ci.nsIUUIDGenerator);
  originalFactoryData.push(registerMockedFactory("@mozilla.org/presentation/datachanneltransportbuilder;1",
                                                 uuidGenerator.generateUUID(),
                                                 mockedSessionTransport));

  addMessageListener('trigger-incoming-message', function(event) {
    mockedSessionTransport.simulateIncomingMessage(event.data.data);
  });
  addMessageListener('teardown', ()=>tearDown());
}

// Exposed to the caller of |loadPrivilegedScript|
var contentScript = {
  handlers: {},
  addMessageListener: function(message, handler) {
    if (this.handlers.hasOwnProperty(message)) {
      this.handlers[message].push(handler);
    } else {
      this.handlers[message] = [handler];
    }
  },
  removeMessageListener: function(message, handler) {
    if (!handler || !this.handlers.hasOwnProperty(message)) {
      return;
    }
    var index = this.handlers[message].indexOf(handler);
    if (index != -1) {
      this.handlers[message].splice(index, 1);
    }
  },
  sendAsyncMessage: function(message, data) {
    port.postMessage({'type': message,
                      'data': data
                     });
  }
}

if (!SpecialPowers.isMainProcess()) {
  var port;
  try {
    port = SpecialPowers.loadPrivilegedScript(loadPrivilegedScriptTest.toSource());
  } catch (e) {
    ok(false, "loadPrivilegedScript shoulde not throw" + e);
  }

  port.onmessage = (e) => {
    var type = e.data['type'];
    if (!contentScript.handlers.hasOwnProperty(type)) {
    return;
    }
    var args = [e.data['data']];
    contentScript.handlers[type].forEach(handler => handler.apply(null, args));
  };
}
