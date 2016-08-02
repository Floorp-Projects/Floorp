/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

const Services = require("Services");

const events = require("sdk/event/core");
const promise = require("promise");
const { on: systemOn, off: systemOff } = require("sdk/system/events");
const protocol = require("devtools/shared/protocol");
const { CallWatcherActor } = require("devtools/server/actors/call-watcher");
const { CallWatcherFront } = require("devtools/shared/fronts/call-watcher");
const { createValueGrip } = require("devtools/server/actors/object");
const AutomationTimeline = require("./utils/automation-timeline");
const { on, once, off, emit } = events;
const { types, method, Arg, Option, RetVal, preEvent } = protocol;
const {
  audionodeSpec,
  webAudioSpec,
  AUTOMATION_METHODS,
  NODE_CREATION_METHODS,
  NODE_ROUTING_METHODS,
} = require("devtools/shared/specs/webaudio");
const { WebAudioFront } = require("devtools/shared/fronts/webaudio");
const AUDIO_NODE_DEFINITION = require("devtools/server/actors/utils/audionodes.json");
const ENABLE_AUTOMATION = false;
const AUTOMATION_GRANULARITY = 2000;
const AUTOMATION_GRANULARITY_MAX = 6000;

const AUDIO_GLOBALS = [
  "AudioContext", "AudioNode", "AudioParam"
];

/**
 * An Audio Node actor allowing communication to a specific audio node in the
 * Audio Context graph.
 */
var AudioNodeActor = exports.AudioNodeActor = protocol.ActorClassWithSpec(audionodeSpec, {
  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID, // actorID is set when this is added to a pool
      type: this.type,
      source: this.source,
      bypassable: this.bypassable,
    };
  },

  /**
   * Create the Audio Node actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param AudioNode node
   *        The AudioNode that was created.
   */
  initialize: function (conn, node) {
    protocol.Actor.prototype.initialize.call(this, conn);

    // Store ChromeOnly property `id` to identify AudioNode,
    // rather than storing a strong reference, and store a weak
    // ref to underlying node for controlling.
    this.nativeID = node.id;
    this.node = Cu.getWeakReference(node);

    // Stores the AutomationTimelines for this node's AudioParams.
    this.automation = {};

    try {
      this.type = getConstructorName(node);
    } catch (e) {
      this.type = "";
    }

    this.source = !!AUDIO_NODE_DEFINITION[this.type].source;
    this.bypassable = !AUDIO_NODE_DEFINITION[this.type].unbypassable;

    // Create automation timelines for all AudioParams
    Object.keys(AUDIO_NODE_DEFINITION[this.type].properties || {})
      .filter(isAudioParam.bind(null, node))
      .forEach(paramName => {
        this.automation[paramName] = new AutomationTimeline(node[paramName].defaultValue);
      });
  },

  /**
   * Returns the string name of the audio type.
   *
   * DEPRECATED: Use `audionode.type` instead, left here for legacy reasons.
   */
  getType: function () {
    return this.type;
  },

  /**
   * Returns a boolean indicating if the AudioNode has been "bypassed",
   * via `AudioNodeActor#bypass` method.
   *
   * @return Boolean
   */
  isBypassed: function () {
    let node = this.node.get();
    if (node === null) {
      return false;
    }

    // Cast to boolean incase `passThrough` is undefined,
    // like for AudioDestinationNode
    return !!node.passThrough;
  },

  /**
   * Takes a boolean, either enabling or disabling the "passThrough" option
   * on an AudioNode. If a node is bypassed, an effects processing node (like gain, biquad),
   * will allow the audio stream to pass through the node, unaffected. Returns
   * the bypass state of the node.
   *
   * @param Boolean enable
   *        Whether the bypass value should be set on or off.
   * @return Boolean
   */
  bypass: function (enable) {
    let node = this.node.get();

    if (node === null) {
      return;
    }

    if (this.bypassable) {
      node.passThrough = enable;
    }

    return this.isBypassed();
  },

  /**
   * Changes a param on the audio node. Responds with either `undefined`
   * on success, or a description of the error upon param set failure.
   *
   * @param String param
   *        Name of the AudioParam to change.
   * @param String value
   *        Value to change AudioParam to.
   */
  setParam: function (param, value) {
    let node = this.node.get();

    if (node === null) {
      return CollectedAudioNodeError();
    }

    try {
      if (isAudioParam(node, param)) {
        node[param].value = value;
        this.automation[param].setValue(value);
      }
      else {
        node[param] = value;
      }
      return undefined;
    } catch (e) {
      return constructError(e);
    }
  },

  /**
   * Gets a param on the audio node.
   *
   * @param String param
   *        Name of the AudioParam to fetch.
   */
  getParam: function (param) {
    let node = this.node.get();

    if (node === null) {
      return CollectedAudioNodeError();
    }

    // Check to see if it's an AudioParam -- if so,
    // return the `value` property of the parameter.
    let value = isAudioParam(node, param) ? node[param].value : node[param];

    // Return the grip form of the value; at this time,
    // there shouldn't be any non-primitives at the moment, other than
    // AudioBuffer or Float32Array references and the like,
    // so this just formats the value to be displayed in the VariablesView,
    // without using real grips and managing via actor pools.
    let grip = createValueGrip(value, null, createObjectGrip);

    return grip;
  },

  /**
   * Get an object containing key-value pairs of additional attributes
   * to be consumed by a front end, like if a property should be read only,
   * or is a special type (Float32Array, Buffer, etc.)
   *
   * @param String param
   *        Name of the AudioParam whose flags are desired.
   */
  getParamFlags: function (param) {
    return ((AUDIO_NODE_DEFINITION[this.type] || {}).properties || {})[param];
  },

  /**
   * Get an array of objects each containing a `param` and `value` property,
   * corresponding to a property name and current value of the audio node.
   */
  getParams: function (param) {
    let props = Object.keys(AUDIO_NODE_DEFINITION[this.type].properties || {});
    return props.map(prop =>
      ({ param: prop, value: this.getParam(prop), flags: this.getParamFlags(prop) }));
  },

  /**
   * Connects this audionode to an AudioParam via `node.connect(param)`.
   */
  connectParam: function (destActor, paramName, output) {
    let srcNode = this.node.get();
    let destNode = destActor.node.get();

    if (srcNode === null || destNode === null) {
      return CollectedAudioNodeError();
    }

    try {
      // Connect via the unwrapped node, so we can call the
      // patched method that fires the webaudio actor's `connect-param` event.
      // Connect directly to the wrapped `destNode`, otherwise
      // the patched method thinks this is a new node and won't be
      // able to find it in `_nativeToActorID`.
      XPCNativeWrapper.unwrap(srcNode).connect(destNode[paramName], output);
    } catch (e) {
      return constructError(e);
    }
  },

  /**
   * Connects this audionode to another via `node.connect(dest)`.
   */
  connectNode: function (destActor, output, input) {
    let srcNode = this.node.get();
    let destNode = destActor.node.get();

    if (srcNode === null || destNode === null) {
      return CollectedAudioNodeError();
    }

    try {
      // Connect via the unwrapped node, so we can call the
      // patched method that fires the webaudio actor's `connect-node` event.
      // Connect directly to the wrapped `destNode`, otherwise
      // the patched method thinks this is a new node and won't be
      // able to find it in `_nativeToActorID`.
      XPCNativeWrapper.unwrap(srcNode).connect(destNode, output, input);
    } catch (e) {
      return constructError(e);
    }
  },

  /**
   * Disconnects this audionode from all connections via `node.disconnect()`.
   */
  disconnect: function (destActor, output) {
    let node = this.node.get();

    if (node === null) {
      return CollectedAudioNodeError();
    }

    try {
      // Disconnect via the unwrapped node, so we can call the
      // patched method that fires the webaudio actor's `disconnect` event.
      XPCNativeWrapper.unwrap(node).disconnect(output);
    } catch (e) {
      return constructError(e);
    }
  },

  getAutomationData: function (paramName) {
    let timeline = this.automation[paramName];
    if (!timeline) {
      return null;
    }

    let events = timeline.events;
    let values = [];
    let i = 0;

    if (!timeline.events.length) {
      return { events, values };
    }

    let firstEvent = events[0];
    let lastEvent = events[timeline.events.length - 1];
    // `setValueCurveAtTime` will have a duration value -- other
    // events will have duration of `0`.
    let timeDelta = (lastEvent.time + lastEvent.duration) - firstEvent.time;
    let scale = timeDelta / AUTOMATION_GRANULARITY;

    for (; i < AUTOMATION_GRANULARITY; i++) {
      let delta = firstEvent.time + (i * scale);
      let value = timeline.getValueAtTime(delta);
      values.push({ delta, value });
    }

    // If the last event is setTargetAtTime, the automation
    // doesn't actually begin until the event's time, and exponentially
    // approaches the target value. In this case, we add more values
    // until we're "close enough" to the target.
    if (lastEvent.type === "setTargetAtTime") {
      for (; i < AUTOMATION_GRANULARITY_MAX; i++) {
        let delta = firstEvent.time + (++i * scale);
        let value = timeline.getValueAtTime(delta);
        values.push({ delta, value });
      }
    }

    return { events, values };
  },

  /**
   * Called via WebAudioActor, registers an automation event
   * for the AudioParam called.
   *
   * @param String paramName
   *        Name of the AudioParam.
   * @param String eventName
   *        Name of the automation event called.
   * @param Array args
   *        Arguments passed into the automation call.
   */
  addAutomationEvent: function (paramName, eventName, args = []) {
    let node = this.node.get();
    let timeline = this.automation[paramName];

    if (node === null) {
      return CollectedAudioNodeError();
    }

    if (!timeline || !node[paramName][eventName]) {
      return InvalidCommandError();
    }

    try {
      // Using the unwrapped node and parameter, the corresponding
      // WebAudioActor event will be fired, subsequently calling
      // `_recordAutomationEvent`. Some finesse is required to handle
      // the cast of TypedArray arguments over the protocol, which is
      // taken care of below. The event will cast the argument back
      // into an array to be broadcasted from WebAudioActor, but the
      // double-casting will only occur when starting from `addAutomationEvent`,
      // which is only used in tests.
      let param = XPCNativeWrapper.unwrap(node[paramName]);
      let contentGlobal = Cu.getGlobalForObject(param);
      let contentArgs = Cu.cloneInto(args, contentGlobal);

      // If calling `setValueCurveAtTime`, the first argument
      // is a Float32Array, which won't be able to be serialized
      // over the protocol. Cast a normal array to a Float32Array here.
      if (eventName === "setValueCurveAtTime") {
        // Create a Float32Array from the content, seeding with an array
        // from the same scope.
        let curve = new contentGlobal.Float32Array(contentArgs[0]);
        contentArgs[0] = curve;
      }

      // Apply the args back from the content scope, which is necessary
      // due to the method wrapping changing in bug 1130901 to be exported
      // directly to the content scope.
      param[eventName].apply(param, contentArgs);
    } catch (e) {
      return constructError(e);
    }
  },

  /**
   * Registers the automation event in the AudioNodeActor's
   * internal timeline. Called when setting automation via
   * `addAutomationEvent`, or from the WebAudioActor's listening
   * to the event firing via content.
   *
   * @param String paramName
   *        Name of the AudioParam.
   * @param String eventName
   *        Name of the automation event called.
   * @param Array args
   *        Arguments passed into the automation call.
   */
  _recordAutomationEvent: function (paramName, eventName, args) {
    let timeline = this.automation[paramName];
    timeline[eventName].apply(timeline, args);
  }
});

/**
 * The Web Audio Actor handles simple interaction with an AudioContext
 * high-level methods. After instantiating this actor, you'll need to set it
 * up by calling setup().
 */
var WebAudioActor = exports.WebAudioActor = protocol.ActorClassWithSpec(webAudioSpec, {
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;

    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);

    // Store ChromeOnly ID (`nativeID` property on AudioNodeActor) mapped
    // to the associated actorID, so we don't have to expose `nativeID`
    // to the client in any way.
    this._nativeToActorID = new Map();

    this._onDestroyNode = this._onDestroyNode.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
  },

  destroy: function (conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Returns definition of all AudioNodes, such as AudioParams, and
   * flags.
   */
  getDefinition: function () {
    return AUDIO_NODE_DEFINITION;
  },

  /**
   * Starts waiting for the current tab actor's document global to be
   * created, in order to instrument the Canvas context and become
   * aware of everything the content does with Web Audio.
   *
   * See ContentObserver and WebAudioInstrumenter for more details.
   */
  setup: function ({ reload }) {
    // Used to track when something is happening with the web audio API
    // the first time, to ultimately fire `start-context` event
    this._firstNodeCreated = false;

    // Clear out stored nativeIDs on reload as we do not want to track
    // AudioNodes that are no longer on this document.
    this._nativeToActorID.clear();

    if (this._initialized) {
      if (reload) {
        this.tabActor.window.location.reload();
      }
      return;
    }

    this._initialized = true;

    this._callWatcher = new CallWatcherActor(this.conn, this.tabActor);
    this._callWatcher.onCall = this._onContentFunctionCall;
    this._callWatcher.setup({
      tracedGlobals: AUDIO_GLOBALS,
      startRecording: true,
      performReload: reload,
      holdWeak: true,
      storeCalls: false
    });
    // Bind to `window-ready` so we can reenable recording on the
    // call watcher
    on(this.tabActor, "window-ready", this._onGlobalCreated);
    // Bind to the `window-destroyed` event so we can unbind events between
    // the global destruction and the `finalize` cleanup method on the actor.
    on(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
  },

  /**
   * Invoked whenever an instrumented function is called, like an AudioContext
   * method or an AudioNode method.
   */
  _onContentFunctionCall: function (functionCall) {
    let { name } = functionCall.details;

    // All Web Audio nodes inherit from AudioNode's prototype, so
    // hook into the `connect` and `disconnect` methods
    if (WebAudioFront.NODE_ROUTING_METHODS.has(name)) {
      this._handleRoutingCall(functionCall);
    }
    else if (WebAudioFront.NODE_CREATION_METHODS.has(name)) {
      this._handleCreationCall(functionCall);
    }
    else if (ENABLE_AUTOMATION && WebAudioFront.AUTOMATION_METHODS.has(name)) {
      this._handleAutomationCall(functionCall);
    }
  },

  _handleRoutingCall: function (functionCall) {
    let { caller, args, name } = functionCall.details;
    let source = caller;
    let dest = args[0];
    let isAudioParam = dest ? getConstructorName(dest) === "AudioParam" : false;

    // audionode.connect(param)
    if (name === "connect" && isAudioParam) {
      this._onConnectParam(source, dest);
    }
    // audionode.connect(node)
    else if (name === "connect") {
      this._onConnectNode(source, dest);
    }
    // audionode.disconnect()
    else if (name === "disconnect") {
      this._onDisconnectNode(source);
    }
  },

  _handleCreationCall: function (functionCall) {
    let { caller, result } = functionCall.details;
    // Keep track of the first node created, so we can alert
    // the front end that an audio context is being used since
    // we're not hooking into the constructor itself, just its
    // instance's methods.
    if (!this._firstNodeCreated) {
      // Fire the start-up event if this is the first node created
      // and trigger a `create-node` event for the context destination
      this._onStartContext();
      this._onCreateNode(caller.destination);
      this._firstNodeCreated = true;
    }
    this._onCreateNode(result);
  },

  _handleAutomationCall: function (functionCall) {
    let { caller, name, args } = functionCall.details;
    let wrappedParam = new XPCNativeWrapper(caller);

    // Sanitize arguments, as these should all be numbers,
    // with the exception of a TypedArray, which needs
    // casted to an Array
    args = sanitizeAutomationArgs(args);

    let nodeActor = this._getActorByNativeID(wrappedParam._parentID);
    nodeActor._recordAutomationEvent(wrappedParam._paramName, name, args);

    this._onAutomationEvent({
      node: nodeActor,
      paramName: wrappedParam._paramName,
      eventName: name,
      args: args
    });
  },

  /**
   * Stops listening for document global changes and puts this actor
   * to hibernation. This method is called automatically just before the
   * actor is destroyed.
   */
  finalize: function () {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;
    systemOff("webaudio-node-demise", this._onDestroyNode);

    off(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
    off(this.tabActor, "window-ready", this._onGlobalCreated);
    this.tabActor = null;
    this._nativeToActorID = null;
    this._callWatcher.eraseRecording();
    this._callWatcher.finalize();
    this._callWatcher = null;
  },

  /**
   * Helper for constructing an AudioNodeActor, assigning to
   * internal weak map, and tracking via `manage` so it is assigned
   * an `actorID`.
   */
  _constructAudioNode: function (node) {
    // Ensure AudioNode is wrapped.
    node = new XPCNativeWrapper(node);

    this._instrumentParams(node);

    let actor = new AudioNodeActor(this.conn, node);
    this.manage(actor);
    this._nativeToActorID.set(node.id, actor.actorID);
    return actor;
  },

  /**
   * Takes an XrayWrapper node, and attaches the node's `nativeID`
   * to the AudioParams as `_parentID`, as well as the the type of param
   * as a string on `_paramName`.
   */
  _instrumentParams: function (node) {
    let type = getConstructorName(node);
    Object.keys(AUDIO_NODE_DEFINITION[type].properties || {})
      .filter(isAudioParam.bind(null, node))
      .forEach(paramName => {
        let param = node[paramName];
        param._parentID = node.id;
        param._paramName = paramName;
      });
  },

  /**
   * Takes an AudioNode and returns the stored actor for it.
   * In some cases, we won't have an actor stored (for example,
   * connecting to an AudioDestinationNode, since it's implicitly
   * created), so make a new actor and store that.
   */
  _getActorByNativeID: function (nativeID) {
    // Ensure we have a Number, rather than a string
    // return via notification.
    nativeID = ~~nativeID;

    let actorID = this._nativeToActorID.get(nativeID);
    let actor = actorID != null ? this.conn.getActor(actorID) : null;
    return actor;
  },

  /**
   * Called on first audio node creation, signifying audio context usage
   */
  _onStartContext: function () {
    systemOn("webaudio-node-demise", this._onDestroyNode);
    emit(this, "start-context");
  },

  /**
   * Called when one audio node is connected to another.
   */
  _onConnectNode: function (source, dest) {
    let sourceActor = this._getActorByNativeID(source.id);
    let destActor = this._getActorByNativeID(dest.id);

    emit(this, "connect-node", {
      source: sourceActor,
      dest: destActor
    });
  },

  /**
   * Called when an audio node is connected to an audio param.
   */
  _onConnectParam: function (source, param) {
    let sourceActor = this._getActorByNativeID(source.id);
    let destActor = this._getActorByNativeID(param._parentID);
    emit(this, "connect-param", {
      source: sourceActor,
      dest: destActor,
      param: param._paramName
    });
  },

  /**
   * Called when an audio node is disconnected.
   */
  _onDisconnectNode: function (node) {
    let actor = this._getActorByNativeID(node.id);
    emit(this, "disconnect-node", actor);
  },

  /**
   * Called when a parameter changes on an audio node
   */
  _onParamChange: function (node, param, value) {
    let actor = this._getActorByNativeID(node.id);
    emit(this, "param-change", {
      source: actor,
      param: param,
      value: value
    });
  },

  /**
   * Called on node creation.
   */
  _onCreateNode: function (node) {
    let actor = this._constructAudioNode(node);
    emit(this, "create-node", actor);
  },

  /** Called when `webaudio-node-demise` is triggered,
   * and emits the associated actor to the front if found.
   */
  _onDestroyNode: function ({data}) {
    // Cast to integer.
    let nativeID = ~~data;

    let actor = this._getActorByNativeID(nativeID);

    // If actorID exists, emit; in the case where we get demise
    // notifications for a document that no longer exists,
    // the mapping should not be found, so we do not emit an event.
    if (actor) {
      this._nativeToActorID.delete(nativeID);
      emit(this, "destroy-node", actor);
    }
  },

  /**
   * Ensures that the new global has recording on
   * so we can proxy the function calls.
   */
  _onGlobalCreated: function () {
    // Used to track when something is happening with the web audio API
    // the first time, to ultimately fire `start-context` event
    this._firstNodeCreated = false;

    // Clear out stored nativeIDs on reload as we do not want to track
    // AudioNodes that are no longer on this document.
    this._nativeToActorID.clear();

    this._callWatcher.resumeRecording();
  },

  /**
   * Fired when an automation event is added to an AudioNode.
   */
  _onAutomationEvent: function ({node, paramName, eventName, args}) {
    emit(this, "automation-event", {
      node: node,
      paramName: paramName,
      eventName: eventName,
      args: args
    });
  },

  /**
   * Called when the underlying ContentObserver fires `global-destroyed`
   * so we can cleanup some things between the global being destroyed and
   * when the actor's `finalize` method gets called.
   */
  _onGlobalDestroyed: function ({id}) {
    if (this._callWatcher._tracedWindowId !== id) {
      return;
    }

    if (this._nativeToActorID) {
      this._nativeToActorID.clear();
    }
    systemOff("webaudio-node-demise", this._onDestroyNode);
  }
});

/**
 * Determines whether or not property is an AudioParam.
 *
 * @param AudioNode node
 *        An AudioNode.
 * @param String prop
 *        Property of `node` to evaluate to see if it's an AudioParam.
 * @return Boolean
 */
function isAudioParam(node, prop) {
  return !!(node[prop] && /AudioParam/.test(node[prop].toString()));
}

/**
 * Takes an `Error` object and constructs a JSON-able response
 *
 * @param Error err
 *        A TypeError, RangeError, etc.
 * @return Object
 */
function constructError(err) {
  return {
    message: err.message,
    type: err.constructor.name
  };
}

/**
 * Creates and returns a JSON-able response used to indicate
 * attempt to access an AudioNode that has been GC'd.
 *
 * @return Object
 */
function CollectedAudioNodeError() {
  return {
    message: "AudioNode has been garbage collected and can no longer be reached.",
    type: "UnreachableAudioNode"
  };
}

function InvalidCommandError() {
  return {
    message: "The command on AudioNode is invalid.",
    type: "InvalidCommand"
  };
}

/**
 * Takes an object and converts it's `toString()` form, like
 * "[object OscillatorNode]" or "[object Float32Array]",
 * or XrayWrapper objects like "[object XrayWrapper [object Array]]"
 * to a string of just the constructor name, like "OscillatorNode",
 * or "Float32Array".
 */
function getConstructorName(obj) {
  return obj.toString().match(/\[object ([^\[\]]*)\]\]?$/)[1];
}

/**
 * Create a grip-like object to pass in renderable information
 * to the front-end for things like Float32Arrays, AudioBuffers,
 * without tracking them in an actor pool.
 */
function createObjectGrip(value) {
  return {
    type: "object",
    preview: {
      kind: "ObjectWithText",
      text: ""
    },
    class: getConstructorName(value)
  };
}

/**
 * Converts all TypedArrays of the array that cannot
 * be passed over the wire into a normal Array equivilent.
 */
function sanitizeAutomationArgs(args) {
  return args.reduce((newArgs, el) => {
    newArgs.push(typeof el === "object" && getConstructorName(el) === "Float32Array" ? castToArray(el) : el);
    return newArgs;
  }, []);
}

/**
 * Casts TypedArray to a normal array via a
 * new scope.
 */
function castToArray(typedArray) {
  // The Xray machinery for TypedArrays denies indexed access on the grounds
  // that it's slow, and advises callers to do a structured clone instead.
  let global = Cu.getGlobalForObject(this);
  let safeView = Cu.cloneInto(typedArray.subarray(), global);
  return copyInto([], safeView);
}

/**
 * Copies values of an array-like `source` into
 * a similarly array-like `dest`.
 */
function copyInto(dest, source) {
  for (let i = 0; i < source.length; i++) {
    dest[i] = source[i];
  }
  return dest;
}
