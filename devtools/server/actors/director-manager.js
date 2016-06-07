/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");

const { Cu, Ci } = require("chrome");

const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal, types } = protocol;

const { sandbox, evaluate } = require("sdk/loader/sandbox");
const { Class } = require("sdk/core/heritage");

const { PlainTextConsole } = require("sdk/console/plain-text");

const { DirectorRegistry } = require("./director-registry");

const {
  messagePortSpec,
  directorManagerSpec,
  directorScriptSpec,
} = require("devtools/shared/specs/director-manager");

/**
 * Error Messages
 */

const ERR_MESSAGEPORT_FINALIZED = "message port finalized";

const ERR_DIRECTOR_UNKNOWN_SCRIPTID = "unkown director-script id";
const ERR_DIRECTOR_UNINSTALLED_SCRIPTID = "uninstalled director-script id";

/**
 * A MessagePort Actor allowing communication through messageport events
 * over the remote debugging protocol.
 */
var MessagePortActor = exports.MessagePortActor = protocol.ActorClassWithSpec(messagePortSpec, {
  /**
   * Create a MessagePort actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param MessagePort port
   *        The wrapped MessagePort.
   */
  initialize: function (conn, port) {
    protocol.Actor.prototype.initialize.call(this, conn);

    // NOTE: can't get a weak reference because we need to subscribe events
    // using port.onmessage or addEventListener
    this.port = port;
  },

  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Sends a message on the wrapped message port.
   *
   * @param Object msg
   *        The JSON serializable message event payload
   */
  postMessage: function (msg) {
    if (!this.port) {
      console.error(ERR_MESSAGEPORT_FINALIZED);
      return;
    }

    this.port.postMessage(msg);
  },

  /**
   * Starts to receive and send queued messages on this message port.
   */
  start: function () {
    if (!this.port) {
      console.error(ERR_MESSAGEPORT_FINALIZED);
      return;
    }

    // NOTE: set port.onmessage to a function is an implicit start
    // and starts to send queued messages.
    // On the client side we should set MessagePortClient.onmessage
    // to a setter which register an handler to the message event
    // and call the actor start method to start receiving messages
    // from the MessagePort's queue.
    this.port.onmessage = (evt) => {
      var ports;

      // TODO: test these wrapped ports
      if (Array.isArray(evt.ports)) {
        ports = evt.ports.map((port) => {
          let actor = new MessagePortActor(this.conn, port);
          this.manage(actor);
          return actor;
        });
      }

      emit(this, "message", {
        isTrusted: evt.isTrusted,
        data: evt.data,
        origin: evt.origin,
        lastEventId: evt.lastEventId,
        source: this,
        ports: ports
      });
    };
  },

  /**
   * Starts to receive and send queued messages on this message port, or
   * raise an exception if the port is null
   */
  close: function () {
    if (!this.port) {
      console.error(ERR_MESSAGEPORT_FINALIZED);
      return;
    }

    this.port.onmessage = null;
    this.port.close();
  },

  finalize: function () {
    this.close();
    this.port = null;
  },
});

/**
 * The Director Script Actor manage javascript code running in a non-privileged sandbox with the same
 * privileges of the target global (browser tab or a firefox os app).
 *
 * After retrieving an instance of this actor (from the tab director actor), you'll need to set it up
 * by calling setup().
 *
 * After the setup, this actor will automatically attach/detach the content script (and optionally a
 * directly connect the debugger client and the content script using a MessageChannel) on tab
 * navigation.
 */
var DirectorScriptActor = exports.DirectorScriptActor = protocol.ActorClassWithSpec(directorScriptSpec, {
  /**
   * Creates the director script actor
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param Actor tabActor
   *        The tab (or root) actor.
   * @param String scriptId
   *        The director-script id.
   * @param String scriptCode
   *        The director-script javascript source.
   * @param Object scriptOptions
   *        The director-script options object.
   */
  initialize: function (conn, tabActor, { scriptId, scriptCode, scriptOptions }) {
    protocol.Actor.prototype.initialize.call(this, conn, tabActor);

    this.tabActor = tabActor;

    this._scriptId = scriptId;
    this._scriptCode = scriptCode;
    this._scriptOptions = scriptOptions;
    this._setupCalled = false;

    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
  },
  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);

    this.finalize();
  },

  /**
   * Starts listening to the tab global created, in order to create the director-script sandbox
   * using the configured scriptCode, attached/detached automatically to the tab
   * window on tab navigation.
   *
   * @param Boolean reload
   *        attach the page immediately or reload it first.
   * @param Boolean skipAttach
   *        skip the attach
   */
  setup: function ({ reload, skipAttach }) {
    if (this._setupCalled) {
      // do nothing
      return;
    }

    this._setupCalled = true;

    on(this.tabActor, "window-ready", this._onGlobalCreated);
    on(this.tabActor, "window-destroyed", this._onGlobalDestroyed);

    // optional skip attach (needed by director-manager for director scripts bulk activation)
    if (skipAttach) {
      return;
    }

    if (reload) {
      this.window.location.reload();
    } else {
      // fake a global created event to attach without reload
      this._onGlobalCreated({ id: getWindowID(this.window), window: this.window, isTopLevel: true });
    }
  },

  /**
   * Get the attached MessagePort actor if any
   */
  getMessagePort: function () {
    return this._messagePortActor;
  },

  /**
   * Stop listening for document global changes, destroy the content worker and puts
   * this actor to hibernation.
   */
  finalize: function () {
    if (!this._setupCalled) {
      return;
    }

    off(this.tabActor, "window-ready", this._onGlobalCreated);
    off(this.tabActor, "window-destroyed", this._onGlobalDestroyed);

    this._onGlobalDestroyed({ id: this._lastAttachedWinId });

    this._setupCalled = false;
  },

  // local helpers
  get window() {
    return this.tabActor.window;
  },

  /* event handlers */
  _onGlobalCreated: function ({ id, window, isTopLevel }) {
    if (!isTopLevel) {
      // filter iframes
      return;
    }

    try {
      if (this._lastAttachedWinId) {
        // if we have received a global created without a previous global destroyed,
        // it's time to cleanup the previous state
        this._onGlobalDestroyed(this._lastAttachedWinId);
      }

      // TODO: check if we want to share a single sandbox per global
      //       for multiple debugger clients

      // create & attach the new sandbox
      this._scriptSandbox = new DirectorScriptSandbox({
        scriptId: this._scriptId,
        scriptCode: this._scriptCode,
        scriptOptions: this._scriptOptions
      });

      // attach the global window
      this._lastAttachedWinId = id;
      var port = this._scriptSandbox.attach(window, id);
      this._onDirectorScriptAttach(window, port);
    } catch (e) {
      this._onDirectorScriptError(e);
    }
  },
  _onGlobalDestroyed: function ({ id }) {
    if (id !== this._lastAttachedWinId) {
       // filter destroyed globals
      return;
    }

     // unmanage and cleanup the messageport actor
    if (this._messagePortActor) {
      this.unmanage(this._messagePortActor);
      this._messagePortActor = null;
    }

     // NOTE: destroy here the old worker
    if (this._scriptSandbox) {
      this._scriptSandbox.destroy(this._onDirectorScriptError.bind(this));

       // send a detach event to the debugger client
      emit(this, "detach", {
        directorScriptId: this._scriptId,
        innerId: this._lastAttachedWinId
      });

      this._lastAttachedWinId = null;
      this._scriptSandbox = null;
    }
  },
  _onDirectorScriptError: function (error) {
    // route the content script error to the debugger client
    if (error) {
      // prevents silent director-script-errors
      console.error("director-script-error", error);
      // route errors to debugger server clients
      emit(this, "error", {
        directorScriptId: this._scriptId,
        message: error.toString(),
        stack: error.stack,
        fileName: error.fileName,
        lineNumber: error.lineNumber,
        columnNumber: error.columnNumber
      });
    }
  },
  _onDirectorScriptAttach: function (window, port) {
    let portActor = new MessagePortActor(this.conn, port);
    this.manage(portActor);
    this._messagePortActor = portActor;

    emit(this, "attach", {
      directorScriptId: this._scriptId,
      url: (window && window.location) ? window.location.toString() : "",
      innerId: this._lastAttachedWinId,
      port: this._messagePortActor
    });
  }
});

/**
 * The DirectorManager Actor is a tab actor which manages enabling/disabling director scripts.
 */
const DirectorManagerActor = exports.DirectorManagerActor = protocol.ActorClassWithSpec(directorManagerSpec, {
  /* init & destroy methods */
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._directorScriptActorsMap = new Map();
  },
  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Retrieves the list of installed director-scripts.
   */
  list: function () {
    let enabledScriptIds = [...this._directorScriptActorsMap.keys()];

    return {
      installed: DirectorRegistry.list(),
      enabled: enabledScriptIds
    };
  },

  /**
   * Bulk enabling director-scripts.
   *
   * @param Array[String] selectedIds
   *        The list of director-script ids to be enabled,
   *        ["*"] will activate all the installed director-scripts
   * @param Boolean reload
   *        optionally reload the target window
   */
  enableByScriptIds: function (selectedIds, { reload }) {
    if (selectedIds && selectedIds.length === 0) {
      // filtered all director scripts ids
      return;
    }

    for (let scriptId of DirectorRegistry.list()) {
      // filter director script ids
      if (selectedIds.indexOf("*") < 0 &&
          selectedIds.indexOf(scriptId) < 0) {
        continue;
      }

      let actor = this.getByScriptId(scriptId);

      // skip attach if reload is true (activated director scripts
      // will be automatically attached on the final reload)
      actor.setup({ reload: false, skipAttach: reload });
    }

    if (reload) {
      this.tabActor.window.location.reload();
    }
  },

  /**
   * Bulk disabling director-scripts.
   *
   * @param Array[String] selectedIds
   *        The list of director-script ids to be disable,
   *        ["*"] will de-activate all the enable director-scripts
   * @param Boolean reload
   *        optionally reload the target window
   */
  disableByScriptIds: function (selectedIds, { reload }) {
    if (selectedIds && selectedIds.length === 0) {
      // filtered all director scripts ids
      return;
    }

    for (let scriptId of this._directorScriptActorsMap.keys()) {
      // filter director script ids
      if (selectedIds.indexOf("*") < 0 &&
          selectedIds.indexOf(scriptId) < 0) {
        continue;
      }

      let actor = this._directorScriptActorsMap.get(scriptId);
      this._directorScriptActorsMap.delete(scriptId);

      // finalize the actor (which will produce director-script-detach event)
      actor.finalize();
      // unsubscribe event handlers on the disabled actor
      off(actor);

      this.unmanage(actor);
    }

    if (reload) {
      this.tabActor.window.location.reload();
    }
  },

  /**
   * Retrieves the actor instance of an installed director-script
   * (and create the actor instance if it doesn't exists yet).
   */
  getByScriptId: function (scriptId) {
    var id = scriptId;
    // raise an unknown director-script id exception
    if (!DirectorRegistry.checkInstalled(id)) {
      console.error(ERR_DIRECTOR_UNKNOWN_SCRIPTID, id);
      throw Error(ERR_DIRECTOR_UNKNOWN_SCRIPTID);
    }

    // get a previous created actor instance
    let actor = this._directorScriptActorsMap.get(id);

    // create a new actor instance
    if (!actor) {
      let directorScriptDefinition = DirectorRegistry.get(id);

      // test lazy director-script (e.g. uninstalled in the parent process)
      if (!directorScriptDefinition) {

        console.error(ERR_DIRECTOR_UNINSTALLED_SCRIPTID, id);
        throw Error(ERR_DIRECTOR_UNINSTALLED_SCRIPTID);
      }

      actor = new DirectorScriptActor(this.conn, this.tabActor, directorScriptDefinition);
      this._directorScriptActorsMap.set(id, actor);

      on(actor, "error", emit.bind(null, this, "director-script-error"));
      on(actor, "attach", emit.bind(null, this, "director-script-attach"));
      on(actor, "detach", emit.bind(null, this, "director-script-detach"));

      this.manage(actor);
    }

    return actor;
  },

  finalize: function () {
    this.disableByScriptIds(["*"], false);
  }
});

/* private helpers */

/**
 * DirectorScriptSandbox is a private utility class, which attach a non-priviliged sandbox
 * to a target window.
 */
const DirectorScriptSandbox = Class({
  initialize: function ({scriptId, scriptCode, scriptOptions}) {
    this._scriptId = scriptId;
    this._scriptCode = scriptCode;
    this._scriptOptions = scriptOptions;
  },

  attach: function (window, innerId) {
    this._innerId = innerId,
    this._window = window;
    this._proto = Cu.createObjectIn(this._window);

    var id = this._scriptId;
    var uri = this._scriptCode;

    this._sandbox = sandbox(window, {
      sandboxName: uri,
      sandboxPrototype: this._proto,
      sameZoneAs: window,
      wantXrays: true,
      wantComponents: false,
      wantExportHelpers: false,
      metadata: {
        URI: uri,
        addonID: id,
        SDKDirectorScript: true,
        "inner-window-id": innerId
      }
    });

    // create a CommonJS module object which match the interface from addon-sdk
    // (addon-sdk/sources/lib/toolkit/loader.js#L678-L686)
    var module = Cu.cloneInto(Object.create(null, {
      id: { enumerable: true, value: id },
      uri: { enumerable: true, value: uri },
      exports: { enumerable: true, value: Cu.createObjectIn(this._sandbox) }
    }), this._sandbox);

    // create a console API object
    let directorScriptConsole = new PlainTextConsole(null, this._innerId);

    // inject CommonJS module globals into the sandbox prototype
    Object.defineProperties(this._proto, {
      module: { enumerable: true, value: module },
      exports: { enumerable: true, value: module.exports },
      console: {
        enumerable: true,
        value: Cu.cloneInto(directorScriptConsole, this._sandbox, { cloneFunctions: true })
      }
    });

    Object.defineProperties(this._sandbox, {
      require: {
        enumerable: true,
        value: Cu.cloneInto(function () {
          throw Error("NOT IMPLEMENTED");
        }, this._sandbox, { cloneFunctions: true })
      }
    });

    // TODO: if the debugger target is local, the debugger client could pass
    // to the director actor the resource url instead of the entire javascript source code.

    // evaluate the director script source in the sandbox
    evaluate(this._sandbox, this._scriptCode, "javascript:" + this._scriptCode);

    // prepare the messageport connected to the debugger client
    let { port1, port2 } = new this._window.MessageChannel();

    // prepare the unload callbacks queue
    var sandboxOnUnloadQueue = this._sandboxOnUnloadQueue = [];

    // create the attach options
    var attachOptions = this._attachOptions = Cu.createObjectIn(this._sandbox);
    Object.defineProperties(attachOptions, {
      port: { enumerable: true, value: port1 },
      window: { enumerable: true, value: window },
      scriptOptions: { enumerable: true, value: Cu.cloneInto(this._scriptOptions, this._sandbox) },
      onUnload: {
        enumerable: true,
        value: Cu.cloneInto(function (cb) {
          // collect unload callbacks
          if (typeof cb == "function") {
            sandboxOnUnloadQueue.push(cb);
          }
        }, this._sandbox, { cloneFunctions: true })
      }
    });

    // select the attach method
    var exports = this._proto.module.exports;
    if (this._scriptOptions && "attachMethod" in this._scriptOptions) {
      this._sandboxOnAttach = exports[this._scriptOptions.attachMethod];
    } else {
      this._sandboxOnAttach = exports;
    }

    if (typeof this._sandboxOnAttach !== "function") {
      throw Error("the configured attachMethod '" +
                  (this._scriptOptions.attachMethod || "module.exports") +
                  "' is not exported by the directorScript");
    }

    // call the attach method
    this._sandboxOnAttach.call(this._sandbox, attachOptions);

    return port2;
  },
  destroy:  function (onError) {
    // evaluate queue unload methods if any
    while (this._sandboxOnUnloadQueue && this._sandboxOnUnloadQueue.length > 0) {
      let cb = this._sandboxOnUnloadQueue.pop();

      try {
        cb();
      } catch (e) {
        console.error("Exception on DirectorScript Sandbox destroy", e);
        onError(e);
      }
    }

    Cu.nukeSandbox(this._sandbox);
  }
});

function getWindowID(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .currentInnerWindowID;
}
