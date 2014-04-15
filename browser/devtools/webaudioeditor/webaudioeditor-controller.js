/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

// Override DOM promises with Promise.jsm helpers
const { defer, all } = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const EventEmitter = require("devtools/toolkit/event-emitter");
const STRINGS_URI = "chrome://browser/locale/devtools/webaudioeditor.properties"
let { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // Fired when the first AudioNode has been created, signifying
  // that the AudioContext is being used and should be tracked via the editor.
  START_CONTEXT: "WebAudioEditor:StartContext",

  // On node creation, connect and disconnect.
  CREATE_NODE: "WebAudioEditor:CreateNode",
  CONNECT_NODE: "WebAudioEditor:ConnectNode",
  DISCONNECT_NODE: "WebAudioEditor:DisconnectNode",

  // When a node gets GC'd.
  DESTROY_NODE: "WebAudioEditor:DestroyNode",

  // On a node parameter's change.
  CHANGE_PARAM: "WebAudioEditor:ChangeParam",

  // When the UI is reset from tab navigation.
  UI_RESET: "WebAudioEditor:UIReset",

  // When a param has been changed via the UI and successfully
  // pushed via the actor to the raw audio node.
  UI_SET_PARAM: "WebAudioEditor:UISetParam",

  // When an audio node is added to the list pane.
  UI_ADD_NODE_LIST: "WebAudioEditor:UIAddNodeList",

  // When the Audio Context graph finishes rendering.
  // Is called with two arguments, first representing number of nodes
  // rendered, second being the number of edges rendered.
  UI_GRAPH_RENDERED: "WebAudioEditor:UIGraphRendered"
};

/**
 * The current target and the Web Audio Editor front, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Track an array of audio nodes
 */
let AudioNodes = [];
let AudioNodeConnections = new WeakMap();


// Light representation wrapping an AudioNode actor with additional properties
function AudioNodeView (actor) {
  this.actor = actor;
  this.id = actor.actorID;
}

// A proxy for the underlying AudioNodeActor to fetch its type
// and subsequently assign the type to the instance.
AudioNodeView.prototype.getType = Task.async(function* () {
  this.type = yield this.actor.getType();
  return this.type;
});

// Helper method to create connections in the AudioNodeConnections
// WeakMap for rendering
AudioNodeView.prototype.connect = function (destination) {
  let connections = AudioNodeConnections.get(this);
  if (!connections) {
    connections = [];
    AudioNodeConnections.set(this, connections);
  }
  connections.push(destination);
};

// Helper method to remove audio connections from the current AudioNodeView
AudioNodeView.prototype.disconnect = function () {
  AudioNodeConnections.set(this, []);
};

// Returns a promise that resolves to an array of objects containing
// both a `param` name property and a `value` property.
AudioNodeView.prototype.getParams = function () {
  return this.actor.getParams();
};


/**
 * Initializes the web audio editor views
 */
function startupWebAudioEditor() {
  return all([
    WebAudioEditorController.initialize(),
    WebAudioGraphView.initialize(),
    WebAudioParamView.initialize()
  ]);
}

/**
 * Destroys the web audio editor controller and views.
 */
function shutdownWebAudioEditor() {
  return all([
    WebAudioEditorController.destroy(),
    WebAudioGraphView.destroy(),
    WebAudioParamView.destroy()
  ]);
}

/**
 * Functions handling target-related lifetime events.
 */
let WebAudioEditorController = {
  /**
   * Listen for events emitted by the current tab target.
   */
  initialize: function() {
    this._onTabNavigated = this._onTabNavigated.bind(this);
    gTarget.on("will-navigate", this._onTabNavigated);
    gTarget.on("navigate", this._onTabNavigated);
    gFront.on("start-context", this._onStartContext);
    gFront.on("create-node", this._onCreateNode);
    gFront.on("connect-node", this._onConnectNode);
    gFront.on("disconnect-node", this._onDisconnectNode);
    gFront.on("change-param", this._onChangeParam);

    // Set up events to refresh the Graph view
    window.on(EVENTS.CREATE_NODE, this._onUpdatedContext);
    window.on(EVENTS.CONNECT_NODE, this._onUpdatedContext);
    window.on(EVENTS.DISCONNECT_NODE, this._onUpdatedContext);
  },

  /**
   * Remove events emitted by the current tab target.
   */
  destroy: function() {
    gTarget.off("will-navigate", this._onTabNavigated);
    gTarget.off("navigate", this._onTabNavigated);
    gFront.off("start-context", this._onStartContext);
    gFront.off("create-node", this._onCreateNode);
    gFront.off("connect-node", this._onConnectNode);
    gFront.off("disconnect-node", this._onDisconnectNode);
    gFront.off("change-param", this._onChangeParam);
    window.off(EVENTS.CREATE_NODE, this._onUpdatedContext);
    window.off(EVENTS.CONNECT_NODE, this._onUpdatedContext);
    window.off(EVENTS.DISCONNECT_NODE, this._onUpdatedContext);
  },

  /**
   * Called when a new audio node is created, or the audio context
   * routing changes.
   */
  _onUpdatedContext: function () {
    WebAudioGraphView.draw();
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabNavigated: function(event) {
    switch (event) {
      case "will-navigate": {
        Task.spawn(function() {
          // Make sure the backend is prepared to handle audio contexts.
          yield gFront.setup({ reload: false });

          // Reset UI to show "Waiting for Audio Context..." and clear out
          // current UI.
          WebAudioGraphView.resetUI();
          WebAudioParamView.resetUI();

          // Clear out stored audio nodes
          AudioNodes.length = 0;
          AudioNodeConnections.clear();
        }).then(() => window.emit(EVENTS.UI_RESET));
        break;
      }
      case "navigate": {
        // TODO Case of bfcache, needs investigating
        // bug 994250
        break;
      }
    }
  },

  /**
   * Called after the first audio node is created in an audio context,
   * signaling that the audio context is being used.
   */
  _onStartContext: function() {
    WebAudioGraphView.showContent();
    window.emit(EVENTS.START_CONTEXT);
  },

  /**
   * Called when a new node is created. Creates an `AudioNodeView` instance
   * for tracking throughout the editor.
   */
  _onCreateNode: Task.async(function* (nodeActor) {
    let node = new AudioNodeView(nodeActor);
    yield node.getType();
    AudioNodes.push(node);
    window.emit(EVENTS.CREATE_NODE, node.id);
  }),

  /**
   * Called when a node is connected to another node.
   */
  _onConnectNode: Task.async(function* ({ source: sourceActor, dest: destActor }) {
    // Since node create and connect are probably executed back to back,
    // and the controller's `_onCreateNode` needs to look up type,
    // the edge creation could be called before the graph node is actually
    // created. This way, we can check and listen for the event before
    // adding an edge.
    let [source, dest] = yield waitForNodeCreation(sourceActor, destActor);

    source.connect(dest);
    window.emit(EVENTS.CONNECT_NODE, source.id, dest.id);

    function waitForNodeCreation (sourceActor, destActor) {
      let deferred = defer();
      let source = getViewNodeByActor(sourceActor);
      let dest = getViewNodeByActor(destActor);

      if (!source || !dest)
        window.on(EVENTS.CREATE_NODE, function createNodeListener (_, id) {
          let createdNode = getViewNodeById(id);
          if (equalActors(sourceActor, createdNode.actor))
            source = createdNode;
          if (equalActors(destActor, createdNode.actor))
            dest = createdNode;
          if (source && dest) {
            window.off(EVENTS.CREATE_NODE, createNodeListener);
            deferred.resolve([source, dest]);
          }
        });
      else
        deferred.resolve([source, dest]);
      return deferred.promise;
    }
  }),

  /**
   * Called when a node is disconnected.
   */
  _onDisconnectNode: function(nodeActor) {
    let node = getViewNodeByActor(nodeActor);
    node.disconnect();
    window.emit(EVENTS.DISCONNECT_NODE, node.id);
  },

  /**
   * Called when a node param is changed.
   */
  _onChangeParam: function({ actor, param, value }) {
    window.emit(EVENTS.CHANGE_PARAM, getViewNodeByActor(actor), param, value);
  }
};

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helper.
 */
function $(selector, target = document) { return target.querySelector(selector); }
function $$(selector, target = document) { return target.querySelectorAll(selector); }

/**
 * Compare `actorID` between two actors to determine if they're corresponding
 * to the same underlying actor.
 */
function equalActors (actor1, actor2) {
  return actor1.actorID === actor2.actorID;
}

/**
 * Returns the corresponding ViewNode by actor
 */
function getViewNodeByActor (actor) {
  for (let i = 0; i < AudioNodes.length; i++) {
    if (equalActors(AudioNodes[i].actor, actor))
      return AudioNodes[i];
  }
  return null;
}

/**
 * Returns the corresponding ViewNode by actorID
 */
function getViewNodeById (id) {
  return getViewNodeByActor({ actorID: id });
}

