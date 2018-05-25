/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("devtools/shared/deprecated-sync-thenables");

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");
const eventSource = require("devtools/shared/client/event-source");
const {ThreadStateTypes} = require("devtools/shared/client/constants");

loader.lazyRequireGetter(this, "ArrayBufferClient", "devtools/shared/client/array-buffer-client");
loader.lazyRequireGetter(this, "EnvironmentClient", "devtools/shared/client/environment-client");
loader.lazyRequireGetter(this, "LongStringClient", "devtools/shared/client/long-string-client");
loader.lazyRequireGetter(this, "ObjectClient", "devtools/shared/client/object-client");
loader.lazyRequireGetter(this, "SourceClient", "devtools/shared/client/source-client");

const noop = () => {};

/**
 * Creates a thread client for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param client DebuggerClient|TabClient
 *        The parent of the thread (tab for tab-scoped debuggers, DebuggerClient
 *        for chrome debuggers).
 * @param actor string
 *        The actor ID for this thread.
 */
function ThreadClient(client, actor) {
  this._parent = client;
  this.client = client instanceof DebuggerClient ? client : client.client;
  this._actor = actor;
  this._frameCache = [];
  this._scriptCache = {};
  this._pauseGrips = {};
  this._threadGrips = {};
  this.request = this.client.request;
}

ThreadClient.prototype = {
  _state: "paused",
  get state() {
    return this._state;
  },
  get paused() {
    return this._state === "paused";
  },

  _pauseOnExceptions: false,
  _ignoreCaughtExceptions: false,
  _pauseOnDOMEvents: null,

  _actor: null,
  get actor() {
    return this._actor;
  },

  get _transport() {
    return this.client._transport;
  },

  _assertPaused: function(command) {
    if (!this.paused) {
      throw Error(command + " command sent while not paused. Currently " + this._state);
    }
  },

  /**
   * Resume a paused thread. If the optional limit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object limit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   * @param function onResponse
   *        Called with the response packet.
   */
  _doResume: DebuggerClient.requester({
    type: "resume",
    resumeLimit: arg(0)
  }, {
    before: function(packet) {
      this._assertPaused("resume");

      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._previousState = this._state;
      this._state = "resuming";

      if (this._pauseOnExceptions) {
        packet.pauseOnExceptions = this._pauseOnExceptions;
      }
      if (this._ignoreCaughtExceptions) {
        packet.ignoreCaughtExceptions = this._ignoreCaughtExceptions;
      }
      if (this._pauseOnDOMEvents) {
        packet.pauseOnDOMEvents = this._pauseOnDOMEvents;
      }
      return packet;
    },
    after: function(response) {
      if (response.error && this._state == "resuming") {
        // There was an error resuming, update the state to the new one
        // reported by the server, if given (only on wrongState), otherwise
        // reset back to the previous state.
        if (response.state) {
          this._state = ThreadStateTypes[response.state];
        } else {
          this._state = this._previousState;
        }
      }
      delete this._previousState;
      return response;
    },
  }),

  /**
   * Reconfigure the thread actor.
   *
   * @param object options
   *        A dictionary object of the new options to use in the thread actor.
   * @param function onResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: arg(0)
  }),

  /**
   * Resume a paused thread.
   */
  resume: function(onResponse) {
    return this._doResume(null, onResponse);
  },

  /**
   * Resume then pause without stepping.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  resumeThenPause: function(onResponse) {
    return this._doResume({ type: "break" }, onResponse);
  },

  /**
   * Step over a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepOver: function(onResponse) {
    return this._doResume({ type: "next" }, onResponse);
  },

  /**
   * Step into a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepIn: function(onResponse) {
    return this._doResume({ type: "step" }, onResponse);
  },

  /**
   * Step out of a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepOut: function(onResponse) {
    return this._doResume({ type: "finish" }, onResponse);
  },

  /**
   * Immediately interrupt a running thread.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  interrupt: function(onResponse) {
    return this._doInterrupt(null, onResponse);
  },

  /**
   * Pause execution right before the next JavaScript bytecode is executed.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  breakOnNext: function(onResponse) {
    return this._doInterrupt("onNext", onResponse);
  },

  /**
   * Interrupt a running thread.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  _doInterrupt: DebuggerClient.requester({
    type: "interrupt",
    when: arg(0)
  }),

  /**
   * Enable or disable pausing when an exception is thrown.
   *
   * @param boolean pauseOnExceptions
   *        Enables pausing if true, disables otherwise.
   * @param boolean ignoreCaughtExceptions
   *        Whether to ignore caught exceptions
   * @param function onResponse
   *        Called with the response packet.
   */
  pauseOnExceptions: function(pauseOnExceptions,
                               ignoreCaughtExceptions,
                               onResponse = noop) {
    this._pauseOnExceptions = pauseOnExceptions;
    this._ignoreCaughtExceptions = ignoreCaughtExceptions;

    // Otherwise send the flag using a standard resume request.
    if (!this.paused) {
      return this.interrupt(response => {
        if (response.error) {
          // Can't continue if pausing failed.
          onResponse(response);
          return response;
        }
        return this.resume(onResponse);
      });
    }

    onResponse();
    return promise.resolve();
  },

  /**
   * Enable pausing when the specified DOM events are triggered. Disabling
   * pausing on an event can be realized by calling this method with the updated
   * array of events that doesn't contain it.
   *
   * @param array|string events
   *        An array of strings, representing the DOM event types to pause on,
   *        or "*" to pause on all DOM events. Pass an empty array to
   *        completely disable pausing on DOM events.
   * @param function onResponse
   *        Called with the response packet in a future turn of the event loop.
   */
  pauseOnDOMEvents: function(events, onResponse = noop) {
    this._pauseOnDOMEvents = events;
    // If the debuggee is paused, the value of the array will be communicated in
    // the next resumption. Otherwise we have to force a pause in order to send
    // the array.
    if (this.paused) {
      DevToolsUtils.executeSoon(() => onResponse({}));
      return {};
    }
    return this.interrupt(response => {
      // Can't continue if pausing failed.
      if (response.error) {
        onResponse(response);
        return response;
      }
      return this.resume(onResponse);
    });
  },

  /**
   * Send a clientEvaluate packet to the debuggee. Response
   * will be a resume packet.
   *
   * @param string frame
   *        The actor ID of the frame where the evaluation should take place.
   * @param string expression
   *        The expression that will be evaluated in the scope of the frame
   *        above.
   * @param function onResponse
   *        Called with the response packet.
   */
  eval: DebuggerClient.requester({
    type: "clientEvaluate",
    frame: arg(0),
    expression: arg(1)
  }, {
    before: function(packet) {
      this._assertPaused("eval");
      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";
      return packet;
    },
    after: function(response) {
      if (response.error) {
        // There was an error resuming, back to paused state.
        this._state = "paused";
      }
      return response;
    },
  }),

  /**
   * Detach from the thread actor.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function(response) {
      this.client.unregisterClient(this);
      this._parent.thread = null;
      return response;
    },
  }),

  /**
   * Release multiple thread-lifetime object actors. If any pause-lifetime
   * actors are included in the request, a |notReleasable| error will return,
   * but all the thread-lifetime ones will have been released.
   *
   * @param array actors
   *        An array with actor IDs to release.
   */
  releaseMany: DebuggerClient.requester({
    type: "releaseMany",
    actors: arg(0),
  }),

  /**
   * Promote multiple pause-lifetime object actors to thread-lifetime ones.
   *
   * @param array actors
   *        An array with actor IDs to promote.
   */
  threadGrips: DebuggerClient.requester({
    type: "threadGrips",
    actors: arg(0)
  }),

  /**
   * Return the event listeners defined on the page.
   *
   * @param onResponse Function
   *        Called with the thread's response.
   */
  eventListeners: DebuggerClient.requester({
    type: "eventListeners"
  }),

  /**
   * Request the loaded sources for the current thread.
   *
   * @param onResponse Function
   *        Called with the thread's response.
   */
  getSources: DebuggerClient.requester({
    type: "sources"
  }),

  /**
   * Clear the thread's source script cache. A scriptscleared event
   * will be sent.
   */
  _clearScripts: function() {
    if (Object.keys(this._scriptCache).length > 0) {
      this._scriptCache = {};
      this.emit("scriptscleared");
    }
  },

  /**
   * Request frames from the callstack for the current thread.
   *
   * @param start integer
   *        The number of the youngest stack frame to return (the youngest
   *        frame is 0).
   * @param count integer
   *        The maximum number of frames to return, or null to return all
   *        frames.
   * @param onResponse function
   *        Called with the thread's response.
   */
  getFrames: DebuggerClient.requester({
    type: "frames",
    start: arg(0),
    count: arg(1)
  }),

  /**
   * Toggle pausing via breakpoints in the server.
   *
   * @param skip boolean
   *        Whether the server should skip pausing via breakpoints
   */
  skipBreakpoints: DebuggerClient.requester({
    type: "skipBreakpoints",
    skip: arg(0),
  }),

  /**
   * An array of cached frames. Clients can observe the framesadded and
   * framescleared event to keep up to date on changes to this cache,
   * and can fill it using the fillFrames method.
   */
  get cachedFrames() {
    return this._frameCache;
  },

  /**
   * true if there are more stack frames available on the server.
   */
  get moreFrames() {
    return this.paused && (!this._frameCache || this._frameCache.length == 0
          || !this._frameCache[this._frameCache.length - 1].oldest);
  },

  /**
   * Request the frame environment.
   *
   * @param frameId string
   */
  getEnvironment: function(frameId) {
    return this.request({ to: frameId, type: "getEnvironment" });
  },

  /**
   * Ensure that at least total stack frames have been loaded in the
   * ThreadClient's stack frame cache. A framesadded event will be
   * sent when the stack frame cache is updated.
   *
   * @param total number
   *        The minimum number of stack frames to be included.
   * @param callback function
   *        Optional callback function called when frames have been loaded
   * @returns true if a framesadded notification should be expected.
   */
  fillFrames: function(total, callback = noop) {
    this._assertPaused("fillFrames");
    if (this._frameCache.length >= total) {
      return false;
    }

    let numFrames = this._frameCache.length;

    this.getFrames(numFrames, total - numFrames, (response) => {
      if (response.error) {
        callback(response);
        return;
      }

      let threadGrips = DevToolsUtils.values(this._threadGrips);

      for (let i in response.frames) {
        let frame = response.frames[i];
        if (!frame.where.source) {
          // Older servers use urls instead, so we need to resolve
          // them to source actors
          for (let grip of threadGrips) {
            if (grip instanceof SourceClient && grip.url === frame.url) {
              frame.where.source = grip._form;
            }
          }
        }

        this._frameCache[frame.depth] = frame;
      }

      // If we got as many frames as we asked for, there might be more
      // frames available.
      this.emit("framesadded");

      callback(response);
    });

    return true;
  },

  /**
   * Clear the thread's stack frame cache. A framescleared event
   * will be sent.
   */
  _clearFrames: function() {
    if (this._frameCache.length > 0) {
      this._frameCache = [];
      this.emit("framescleared");
    }
  },

  /**
   * Return a ObjectClient object for the given object grip.
   *
   * @param grip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip: function(grip) {
    if (grip.actor in this._pauseGrips) {
      return this._pauseGrips[grip.actor];
    }

    let client = new ObjectClient(this.client, grip);
    this._pauseGrips[grip.actor] = client;
    return client;
  },

  /**
   * Get or create a long string client, checking the grip client cache if it
   * already exists.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   * @param gripCacheName String
   *        The property name of the grip client cache to check for existing
   *        clients in.
   */
  _longString: function(grip, gripCacheName) {
    if (grip.actor in this[gripCacheName]) {
      return this[gripCacheName][grip.actor];
    }

    let client = new LongStringClient(this.client, grip);
    this[gripCacheName][grip.actor] = client;
    return client;
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the current pause.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   */
  pauseLongString: function(grip) {
    return this._longString(grip, "_pauseGrips");
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the thread lifetime.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   */
  threadLongString: function(grip) {
    return this._longString(grip, "_threadGrips");
  },

  /**
   * Get or create an ArrayBuffer client, checking the grip client cache if it
   * already exists.
   *
   * @param grip Object
   *        The ArrayBuffer grip returned by the protocol.
   * @param gripCacheName String
   *        The property name of the grip client cache to check for existing
   *        clients in.
   */
  _arrayBuffer: function(grip, gripCacheName) {
    if (grip.actor in this[gripCacheName]) {
      return this[gripCacheName][grip.actor];
    }

    let client = new ArrayBufferClient(this.client, grip);
    this[gripCacheName][grip.actor] = client;
    return client;
  },

  /**
   * Return an instance of ArrayBufferClient for the given ArrayBuffer grip that
   * is scoped to the thread lifetime.
   *
   * @param grip Object
   *        The ArrayBuffer grip returned by the protocol.
   */
  threadArrayBuffer: function(grip) {
    return this._arrayBuffer(grip, "_threadGrips");
  },

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param gripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectClients: function(gripCacheName) {
    for (let id in this[gripCacheName]) {
      this[gripCacheName][id].valid = false;
    }
    this[gripCacheName] = {};
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearPauseGrips: function() {
    this._clearObjectClients("_pauseGrips");
  },

  /**
   * Invalidate thread-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearThreadGrips: function() {
    this._clearObjectClients("_threadGrips");
  },

  /**
   * Handle thread state change by doing necessary cleanup and notifying all
   * registered listeners.
   */
  _onThreadState: function(packet) {
    this._state = ThreadStateTypes[packet.type];
    // The debugger UI may not be initialized yet so we want to keep
    // the packet around so it knows what to pause state to display
    // when it's initialized
    this._lastPausePacket = packet.type === "resumed" ? null : packet;
    this._clearFrames();
    this._clearPauseGrips();
    packet.type === ThreadStateTypes.detached && this._clearThreadGrips();
    this.client._eventsEnabled && this.emit(packet.type, packet);
  },

  getLastPausePacket: function() {
    return this._lastPausePacket;
  },

  /**
   * Return an EnvironmentClient instance for the given environment actor form.
   */
  environment: function(form) {
    return new EnvironmentClient(this.client, form);
  },

  /**
   * Return an instance of SourceClient for the given source actor form.
   */
  source: function(form) {
    if (form.actor in this._threadGrips) {
      return this._threadGrips[form.actor];
    }

    this._threadGrips[form.actor] = new SourceClient(this, form);
    return this._threadGrips[form.actor];
  },

  /**
   * Request the prototype and own properties of mutlipleObjects.
   *
   * @param onResponse function
   *        Called with the request's response.
   * @param actors [string]
   *        List of actor ID of the queried objects.
   */
  getPrototypesAndProperties: DebuggerClient.requester({
    type: "prototypesAndProperties",
    actors: arg(0)
  }),

  events: ["newSource"]
};

eventSource(ThreadClient.prototype);

module.exports = ThreadClient;
