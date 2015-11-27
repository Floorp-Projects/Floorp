"use strict";

var { Ci, Cu } = require("chrome");
var { DebuggerServer } = require("devtools/server/main");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this, "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
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

function WorkerActor(dbg) {
  this._dbg = dbg;
  this._isAttached = false;
  this._threadActor = null;
  this._transport = null;
}

WorkerActor.prototype = {
  actorPrefix: "worker",

  form: function () {
    return {
      actor: this.actorID,
      consoleActor: this._consoleActor,
      url: this._dbg.url,
      type: this._dbg.type
    };
  },

  onAttach: function () {
    if (this._dbg.isClosed) {
      return { error: "closed" };
    }

    if (!this._isAttached) {
      this._dbg.addListener(this);
      this._isAttached = true;
    }

    return {
      type: "attached",
      url: this._dbg.url
    };
  },

  onDetach: function () {
    if (!this._isAttached) {
      return { error: "wrongState" };
    }

    this._detach();

    return { type: "detached" };
  },

  onConnect: function (request) {
    if (!this._isAttached) {
      return { error: "wrongState" };
    }

    if (this._threadActor !== null) {
      return {
        type: "connected",
        threadActor: this._threadActor
      };
    }

    return DebuggerServer.connectToWorker(
      this.conn, this._dbg, this.actorID, request.options
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
  },

  onClose: function () {
    if (this._isAttached) {
      this._detach();
    }

    this.conn.sendActorEvent(this.actorID, "close");
  },

  onError: function (filename, lineno, message) {
    reportError("ERROR:" + filename + ":" + lineno + ":" + message + "\n");
  },

  _detach: function () {
    if (this._threadActor !== null) {
      this._transport.close();
      this._transport = null;
      this._threadActor = null;
    }

    this._dbg.removeListener(this);
    this._isAttached = false;
  }
};

WorkerActor.prototype.requestTypes = {
  "attach": WorkerActor.prototype.onAttach,
  "detach": WorkerActor.prototype.onDetach,
  "connect": WorkerActor.prototype.onConnect
};

exports.WorkerActor = WorkerActor;

function WorkerActorList(options) {
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
        this._actors.set(dbg, new WorkerActor(dbg));
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

function ServiceWorkerRegistrationActor(registration) {
  this._registration = registration;
};

ServiceWorkerRegistrationActor.prototype = {
  actorPrefix: "serviceWorkerRegistration",

  form: function () {
    return {
      actor: this.actorID,
      scope: this._registration.scope
    };
  }
};

function ServiceWorkerRegistrationActorList() {
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
          new ServiceWorkerRegistrationActor(registration));
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
