/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

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
      sendAsyncMessage(type, { data });
    } else {
      port.postMessage({ type, data });
    }
  }

  if (typeof port != "undefined") {
    /**
     * When the script is loaded by |loadPrivilegedScript|, these APIs
     * are exposed to this script.
     */
    port.onmessage = e => {
      var type = e.data.type;
      if (!handlers.hasOwnProperty(type)) {
        return;
      }
      var args = [e];
      handlers[type].forEach(handler => handler.apply(null, args));
    };
    var handlers = {};
    /* eslint-disable-next-line no-global-assign */
    addMessageListener = function(message, handler) {
      if (handlers.hasOwnProperty(message)) {
        handlers[message].push(handler);
      } else {
        handlers[message] = [handler];
      }
    };
    /* eslint-disable-next-line no-global-assign */
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

  const Cm = Components.manager;

  const mockedChannelDescription = {
    /* eslint-disable-next-line mozilla/use-chromeutils-generateqi */
    QueryInterface(iid) {
      const interfaces = [Ci.nsIPresentationChannelDescription];

      if (!interfaces.some(v => iid.equals(v))) {
        throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
      }
      return this;
    },
    get type() {
      /* global Services */
      if (
        Services.prefs.getBoolPref(
          "dom.presentation.session_transport.data_channel.enable"
        )
      ) {
        return Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL;
      }
      return Ci.nsIPresentationChannelDescription.TYPE_TCP;
    },
    get dataChannelSDP() {
      return "test-sdp";
    },
  };

  function setTimeout(callback, delay) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      { notify: callback },
      delay,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
    return timer;
  }

  const mockedSessionTransport = {
    /* eslint-disable-next-line mozilla/use-chromeutils-generateqi */
    QueryInterface(iid) {
      const interfaces = [
        Ci.nsIPresentationSessionTransport,
        Ci.nsIPresentationDataChannelSessionTransportBuilder,
        Ci.nsIFactory,
      ];

      if (!interfaces.some(v => iid.equals(v))) {
        throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
      }
      return this;
    },
    createInstance(aOuter, aIID) {
      if (aOuter) {
        throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
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
    buildDataChannelTransport(role, window, listener) {
      dump("PresentationSessionFrameScript: build data channel transport\n");
      this._listener = listener;
      this._role = role;

      var hasNavigator = window
        ? typeof window.navigator != "undefined"
        : false;
      sendMessage("check-navigator", hasNavigator);

      if (this._role == Ci.nsIPresentationService.ROLE_CONTROLLER) {
        this._listener.sendOffer(mockedChannelDescription);
      }
    },

    enableDataNotification() {
      sendMessage("data-transport-notification-enabled");
    },
    send(data) {
      sendMessage("message-sent", data);
    },
    close(reason) {
      sendMessage("data-transport-closed", reason);
      this._callback
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyTransportClosed(reason);
      this._callback = null;
    },
    simulateTransportReady() {
      this._callback
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyTransportReady();
    },
    simulateIncomingMessage(message) {
      this._callback
        .QueryInterface(Ci.nsIPresentationSessionTransportCallback)
        .notifyData(message, false);
    },
    onOffer(aOffer) {
      this._listener.sendAnswer(mockedChannelDescription);
      this._onSessionTransport();
    },
    onAnswer(aAnswer) {
      this._onSessionTransport();
    },
    _onSessionTransport() {
      setTimeout(() => {
        this._listener.onSessionTransport(this);
        this.simulateTransportReady();
        this._listener = null;
      }, 0);
    },
  };

  function tearDown() {
    mockedSessionTransport.callback = null;

    /* Register original factories. */
    for (var data of originalFactoryData) {
      registerOriginalFactory(
        data.contractId,
        data.mockedClassId,
        data.mockedFactory,
        data.originalClassId,
        data.originalFactory
      );
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
      registrar.registerFactory(mockedClassId, "", contractId, mockedFactory);
    }

    return {
      contractId,
      mockedClassId,
      mockedFactory,
      originalClassId,
      originalFactory,
    };
  }

  function registerOriginalFactory(
    contractId,
    mockedClassId,
    mockedFactory,
    originalClassId,
    originalFactory
  ) {
    if (originalFactory) {
      var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
      registrar.unregisterFactory(mockedClassId, mockedFactory);
      registrar.registerFactory(originalClassId, "", contractId, null);
    }
  }

  /* Register mocked factories. */
  const originalFactoryData = [];
  const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  originalFactoryData.push(
    registerMockedFactory(
      "@mozilla.org/presentation/datachanneltransportbuilder;1",
      uuidGenerator.generateUUID(),
      mockedSessionTransport
    )
  );

  addMessageListener("trigger-incoming-message", function(event) {
    mockedSessionTransport.simulateIncomingMessage(event.data.data);
  });
  addMessageListener("teardown", () => tearDown());
}

// Exposed to the caller of |loadPrivilegedScript|
var contentScript = {
  handlers: {},
  addMessageListener(message, handler) {
    if (this.handlers.hasOwnProperty(message)) {
      this.handlers[message].push(handler);
    } else {
      this.handlers[message] = [handler];
    }
  },
  removeMessageListener(message, handler) {
    if (!handler || !this.handlers.hasOwnProperty(message)) {
      return;
    }
    var index = this.handlers[message].indexOf(handler);
    if (index != -1) {
      this.handlers[message].splice(index, 1);
    }
  },
  sendAsyncMessage(message, data) {
    port.postMessage({ type: message, data });
  },
};

if (!SpecialPowers.isMainProcess()) {
  var port;
  try {
    port = SpecialPowers.loadPrivilegedScript(
      loadPrivilegedScriptTest.toString()
    );
  } catch (e) {
    ok(false, "loadPrivilegedScript should not throw" + e);
  }

  port.onmessage = e => {
    var type = e.data.type;
    if (!contentScript.handlers.hasOwnProperty(type)) {
      return;
    }
    var args = [e.data.data];
    contentScript.handlers[type].forEach(handler => handler.apply(null, args));
  };
}
