"use strict";

var { Ci, Cu } = require("chrome");
var { DebuggerServer } = require("devtools/server/main");
var Services = require("Services");
const protocol = require("devtools/shared/protocol");
const { Arg, method, RetVal } = protocol;

loader.lazyRequireGetter(this, "ChromeUtils");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this, "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this, "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

function matchWorkerDebugger(dbg, options) {
  if ("type" in options && dbg.type !== options.type) {
    return false;
  }
  if ("window" in options) {
    let window = dbg.window;
    while (window !== null && window.parent !== window) {
      window = window.parent;
    }

    if (window !== options.window) {
      return false;
    }
  }

  return true;
}

let WorkerActor = protocol.ActorClass({
  typeName: "worker",

  initialize: function (conn, dbg) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._dbg = dbg;
    this._attached = false;
    this._threadActor = null;
    this._transport = null;
    this.manage(this);
  },

  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    let form = {
      actor: this.actorID,
      consoleActor: this._consoleActor,
      url: this._dbg.url,
      type: this._dbg.type
    };
    if (this._dbg.type === Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      let registration = this._getServiceWorkerRegistrationInfo();
      form.scope = registration.scope;
    }
    return form;
  },

  attach: method(function () {
    if (this._dbg.isClosed) {
      return { error: "closed" };
    }

    if (!this._attached) {
      // Automatically disable their internal timeout that shut them down
      // Should be refactored by having actors specific to service workers
      if (this._dbg.type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
        let worker = this._getServiceWorkerInfo();
        if (worker) {
          worker.attachDebugger();
        }
      }
      this._dbg.addListener(this);
      this._attached = true;
    }

    return {
      type: "attached",
      url: this._dbg.url
    };
  }, {
    request: {},
    response: RetVal("json")
  }),

  detach: method(function () {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    this._detach();

    return { type: "detached" };
  }, {
    request: {},
    response: RetVal("json")
  }),

  connect: method(function (options) {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    if (this._threadActor !== null) {
      return {
        type: "connected",
        threadActor: this._threadActor
      };
    }

    return DebuggerServer.connectToWorker(
      this.conn, this._dbg, this.actorID, options
    ).then(({ threadActor, transport, consoleActor }) => {
      this._threadActor = threadActor;
      this._transport = transport;
      this._consoleActor = consoleActor;

      return {
        type: "connected",
        threadActor: this._threadActor,
        consoleActor: this._consoleActor
      };
    }, (error) => {
      return { error: error.toString() };
    });
  }, {
    request: {
      options: Arg(0, "json"),
    },
    response: RetVal("json")
  }),

  push: method(function () {
    if (this._dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return { error: "wrongType" };
    }
    let registration = this._getServiceWorkerRegistrationInfo();
    let originAttributes = ChromeUtils.originAttributesToSuffix(
      this._dbg.principal.originAttributes);
    swm.sendPushEvent(originAttributes, registration.scope);
    return { type: "pushed" };
  }, {
    request: {},
    response: RetVal("json")
  }),

  onClose: function () {
    if (this._attached) {
      this._detach();
    }

    this.conn.sendActorEvent(this.actorID, "close");
  },

  onError: function (filename, lineno, message) {
    reportError("ERROR:" + filename + ":" + lineno + ":" + message + "\n");
  },

  _getServiceWorkerRegistrationInfo() {
    return swm.getRegistrationByPrincipal(this._dbg.principal, this._dbg.url);
  },

  _getServiceWorkerInfo: function () {
    let registration = this._getServiceWorkerRegistrationInfo();
    return registration.getWorkerByID(this._dbg.serviceWorkerID);
  },

  _detach: function () {
    if (this._threadActor !== null) {
      this._transport.close();
      this._transport = null;
      this._threadActor = null;
    }

    // If the worker is already destroyed, nsIWorkerDebugger.type throws
    // (_dbg.closed appears to be false when it throws)
    let type;
    try {
      type = this._dbg.type;
    } catch(e) {}

    if (type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      let worker = this._getServiceWorkerInfo();
      if (worker) {
        worker.detachDebugger();
      }
    }

    this._dbg.removeListener(this);
    this._attached = false;
  }
});

exports.WorkerActor = WorkerActor;

function WorkerActorList(conn, options) {
  this._conn = conn;
  this._options = options;
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;
  this.onRegister = this.onRegister.bind(this);
  this.onUnregister = this.onUnregister.bind(this);
}

WorkerActorList.prototype = {
  getList: function () {
    // Create a set of debuggers.
    let dbgs = new Set();
    let e = wdm.getWorkerDebuggerEnumerator();
    while (e.hasMoreElements()) {
      let dbg = e.getNext().QueryInterface(Ci.nsIWorkerDebugger);
      if (matchWorkerDebugger(dbg, this._options)) {
        dbgs.add(dbg);
      }
    }

    // Delete each actor for which we don't have a debugger.
    for (let [dbg, ] of this._actors) {
      if (!dbgs.has(dbg)) {
        this._actors.delete(dbg);
      }
    }

    // Create an actor for each debugger for which we don't have one.
    for (let dbg of dbgs) {
      if (!this._actors.has(dbg)) {
        this._actors.set(dbg, new WorkerActor(this._conn, dbg));
      }
    }

    let actors = [];
    for (let [, actor] of this._actors) {
      actors.push(actor);
    }

    if (!this._mustNotify) {
      if (this._onListChanged !== null) {
        wdm.addListener(this);
      }
      this._mustNotify = true;
    }

    return Promise.resolve(actors);
  },

  get onListChanged() {
    return this._onListChanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }
    if (onListChanged === this._onListChanged) {
      return;
    }

    if (this._mustNotify) {
      if (this._onListChanged === null && onListChanged !== null) {
        wdm.addListener(this);
      }
      if (this._onListChanged !== null && onListChanged === null) {
        wdm.removeListener(this);
      }
    }
    this._onListChanged = onListChanged;
  },

  _notifyListChanged: function () {
     this._onListChanged();

     if (this._onListChanged !== null) {
       wdm.removeListener(this);
     }
     this._mustNotify = false;
  },

  onRegister: function (dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  },

  onUnregister: function (dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  }
};

exports.WorkerActorList = WorkerActorList;

// Lazily load the service-worker-child.js process script only once.
let _serviceWorkerProcessScriptLoaded = false;

let ServiceWorkerRegistrationActor = protocol.ActorClass({
  typeName: "serviceWorkerRegistration",

  initialize: function(conn, registration) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._registration = registration;
    this.manage(this);
  },

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    return {
      actor: this.actorID,
      scope: this._registration.scope,
      url: this._registration.scriptSpec
    };
  },

  start: method(function() {
    if (!_serviceWorkerProcessScriptLoaded) {
      Services.ppmm.loadProcessScript(
        "resource://devtools/server/service-worker-child.js", true);
      _serviceWorkerProcessScriptLoaded = true;
    }
    Services.ppmm.broadcastAsyncMessage("serviceWorkerRegistration:start", {
      scope: this._registration.scope
    });
    return { type: "started" };
  }, {
    request: {},
    response: RetVal("json")
  }),

  unregister: method(function() {
    let { principal, scope } = this._registration;
    let unregisterCallback = {
      unregisterSucceeded: function() {},
      unregisterFailed: function() {
        console.error("Failed to unregister the service worker for " + scope);
      },
      QueryInterface: XPCOMUtils.generateQI(
        [Ci.nsIServiceWorkerUnregisterCallback])
    };
    swm.propagateUnregister(principal, unregisterCallback, scope);

    return { type: "unregistered" };
  }, {
    request: {},
    response: RetVal("json")
  }),
});

function ServiceWorkerRegistrationActorList(conn) {
  this._conn = conn;
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;
  this.onRegister = this.onRegister.bind(this);
  this.onUnregister = this.onUnregister.bind(this);
};

ServiceWorkerRegistrationActorList.prototype = {
  getList: function () {
    // Create a set of registrations.
    let registrations = new Set();
    let array = swm.getAllRegistrations();
    for (let index = 0; index < array.length; ++index) {
      registrations.add(
        array.queryElementAt(index, Ci.nsIServiceWorkerRegistrationInfo));
    }

    // Delete each actor for which we don't have a registration.
    for (let [registration, ] of this._actors) {
      if (!registrations.has(registration)) {
        this._actors.delete(registration);
      }
    }

    // Create an actor for each registration for which we don't have one.
    for (let registration of registrations) {
      if (!this._actors.has(registration)) {
        this._actors.set(registration,
          new ServiceWorkerRegistrationActor(this._conn, registration));
      }
    }

    if (!this._mustNotify) {
      if (this._onListChanged !== null) {
        swm.addListener(this);
      }
      this._mustNotify = true;
    }

    let actors = [];
    for (let [, actor] of this._actors) {
      actors.push(actor);
    }

    return Promise.resolve(actors);
  },

  get onListchanged() {
    return this._onListchanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }

    if (this._mustNotify) {
      if (this._onListChanged === null && onListChanged !== null) {
        swm.addListener(this);
      }
      if (this._onListChanged !== null && onListChanged === null) {
        swm.removeListener(this);
      }
    }
    this._onListChanged = onListChanged;
  },

  _notifyListChanged: function () {
    this._onListChanged();

    if (this._onListChanged !== null) {
      swm.removeListener(this);
    }
    this._mustNotify = false;
  },

  onRegister: function (registration) {
    this._notifyListChanged();
  },

  onUnregister: function (registration) {
    this._notifyListChanged();
  }
};

exports.ServiceWorkerRegistrationActorList = ServiceWorkerRegistrationActorList;
