/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A collection of `AudioNodeModel`s used throughout the editor
 * to keep track of audio nodes within the audio context.
 */
let gAudioNodes = new AudioNodesCollection();

/**
 * Initializes the web audio editor views
 */
function startupWebAudioEditor() {
  return all([
    WebAudioEditorController.initialize(),
    ContextView.initialize(),
    InspectorView.initialize(),
    PropertiesView.initialize(),
    AutomationView.initialize()
  ]);
}

/**
 * Destroys the web audio editor controller and views.
 */
function shutdownWebAudioEditor() {
  return all([
    WebAudioEditorController.destroy(),
    ContextView.destroy(),
    InspectorView.destroy(),
    PropertiesView.destroy(),
    AutomationView.destroy()
  ]);
}

/**
 * Functions handling target-related lifetime events.
 */
let WebAudioEditorController = {
  /**
   * Listen for events emitted by the current tab target.
   */
  initialize: Task.async(function* () {
    // Create a queue to manage all the events from the
    // front so they can be executed in order
    this.queue = new Queue();

    telemetry.toolOpened("webaudioeditor");
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onThemeChange = this._onThemeChange.bind(this);
    this._onStartContext = this._onStartContext.bind(this);

    this._onCreateNode = this.queue.addHandler(this._onCreateNode.bind(this));
    this._onConnectNode = this.queue.addHandler(this._onConnectNode.bind(this));
    this._onConnectParam = this.queue.addHandler(this._onConnectParam.bind(this));
    this._onDisconnectNode = this.queue.addHandler(this._onDisconnectNode.bind(this));
    this._onChangeParam = this.queue.addHandler(this._onChangeParam.bind(this));
    this._onDestroyNode = this.queue.addHandler(this._onDestroyNode.bind(this));

    gTarget.on("will-navigate", this._onTabNavigated);
    gTarget.on("navigate", this._onTabNavigated);
    gFront.on("start-context", this._onStartContext);
    gFront.on("create-node", this._onCreateNode);
    gFront.on("connect-node", this._onConnectNode);
    gFront.on("connect-param", this._onConnectParam);
    gFront.on("disconnect-node", this._onDisconnectNode);
    gFront.on("change-param", this._onChangeParam);
    gFront.on("destroy-node", this._onDestroyNode);

    // Hook into theme change so we can change
    // the graph's marker styling, since we can't do this
    // with CSS
    gDevTools.on("pref-changed", this._onThemeChange);

    // Store the AudioNode definitions from the WebAudioFront, if the method exists.
    // If not, get the JSON directly. Using the actor method is preferable so the client
    // knows exactly what methods are supported on the server.
    let actorHasDefinition = yield gTarget.actorHasMethod("webaudio", "getDefinition");
    if (actorHasDefinition) {
      AUDIO_NODE_DEFINITION = yield gFront.getDefinition();
    } else {
      AUDIO_NODE_DEFINITION = require("devtools/server/actors/utils/audionodes.json");
    }
  }),

  /**
   * Remove events emitted by the current tab target.
   */
  destroy: Task.async(function*() {
    telemetry.toolClosed("webaudioeditor");
    gTarget.off("will-navigate", this._onTabNavigated);
    gTarget.off("navigate", this._onTabNavigated);
    gFront.off("start-context", this._onStartContext);
    gFront.off("create-node", this._onCreateNode);
    gFront.off("connect-node", this._onConnectNode);
    gFront.off("connect-param", this._onConnectParam);
    gFront.off("disconnect-node", this._onDisconnectNode);
    gFront.off("change-param", this._onChangeParam);
    gFront.off("destroy-node", this._onDestroyNode);
    gDevTools.off("pref-changed", this._onThemeChange);
    yield this.queue.clear();
    this.queue = null;
  }),

  /**
   * Called when page is reloaded to show the reload notice and waiting
   * for an audio context notice.
   */
  reset: function () {
    $("#content").hidden = true;
    ContextView.resetUI();
    InspectorView.resetUI();
    PropertiesView.resetUI();
  },

  /**
   * Takes an AudioNodeActor and returns the corresponding AudioNodeModel.
   */
  getNode: function (nodeActor) {
    let id = nodeActor.actorID;
    let node = gAudioNodes.get(id);
    return node;
  },

  /**
   * Fired when the devtools theme changes (light, dark, etc.)
   * so that the graph can update marker styling, as that
   * cannot currently be done with CSS.
   */
  _onThemeChange: function (event, data) {
    window.emit(EVENTS.THEME_CHANGE, data.newValue);
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabNavigated: Task.async(function* (event, {isFrameSwitching}) {
    switch (event) {
      case "will-navigate": {
        yield this.queue.clear();
        gAudioNodes.reset();

        // Make sure the backend is prepared to handle audio contexts.
        if (!isFrameSwitching) {
          yield gFront.setup({ reload: false });
        }

        // Clear out current UI.
        yield this.reset();

        // When switching to an iframe, ensure displaying the reload button.
        // As the document has already been loaded without being hooked.
        if (isFrameSwitching) {
          $("#reload-notice").hidden = false;
          $("#waiting-notice").hidden = true;
        } else {
          // Otherwise, we are loading a new top level document,
          // so we don't need to reload anymore and should receive
          // new node events.
          $("#reload-notice").hidden = true;
          $("#waiting-notice").hidden = false;
        }

        window.emit(EVENTS.UI_RESET);
        break;
      }
      case "navigate": {
        // TODO Case of bfcache, needs investigating
        // bug 994250
        break;
      }
    }
  }),

  /**
   * Called after the first audio node is created in an audio context,
   * signaling that the audio context is being used.
   */
  _onStartContext: function() {
    $("#reload-notice").hidden = true;
    $("#waiting-notice").hidden = true;
    $("#content").hidden = false;
    window.emit(EVENTS.START_CONTEXT);
  },

  /**
   * Called when a new node is created. Creates an `AudioNodeModel` instance
   * for tracking throughout the editor.
   */
  _onCreateNode: Task.async(function* (nodeActor) {
    yield gAudioNodes.add(nodeActor);
  }),

  /**
   * Called on `destroy-node` when an AudioNode is GC'd. Removes
   * from the AudioNode array and fires an event indicating the removal.
   */
  _onDestroyNode: function (nodeActor) {
    gAudioNodes.remove(gAudioNodes.get(nodeActor.actorID));
  },

  /**
   * Called when a node is connected to another node.
   */
  _onConnectNode: function ({ source: sourceActor, dest: destActor }) {
    let source = this.getNode(sourceActor);
    let dest = this.getNode(destActor);
    source.connect(dest);
  },

  /**
   * Called when a node is conneceted to another node's AudioParam.
   */
  _onConnectParam: function ({ source: sourceActor, dest: destActor, param }) {
    let source = this.getNode(sourceActor);
    let dest = this.getNode(destActor);
    source.connect(dest, param);
  },

  /**
   * Called when a node is disconnected.
   */
  _onDisconnectNode: function (nodeActor) {
    let node = this.getNode(nodeActor);
    node.disconnect();
  },

  /**
   * Called when a node param is changed.
   */
  _onChangeParam: function ({ actor, param, value }) {
    let node = this.getNode(actor);
    window.emit(EVENTS.CHANGE_PARAM, node, param, value);
  }
};
