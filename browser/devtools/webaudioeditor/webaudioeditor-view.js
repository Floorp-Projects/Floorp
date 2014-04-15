/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/VariablesViewController.jsm");
const { debounce } = require("sdk/lang/functional");

// Globals for d3 stuff
// Width/height in pixels of SVG graph
// TODO investigate to see how this works in other host types bug 994257
const WIDTH = 1000;
const HEIGHT = 400;

// Sizes of SVG arrows in graph
const ARROW_HEIGHT = 5;
const ARROW_WIDTH = 8;

const GRAPH_DEBOUNCE_TIMER = 100;

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  lazyEmpty: true,
  lazyEmptyDelay: 10, // ms
  searchEnabled: false,
  editableValueTooltip: "",
  editableNameTooltip: "",
  preventDisableOnChange: true,
  preventDescriptorModifiers: true,
  eval: () => {}
};

/**
 * Functions handling the graph UI.
 */
let WebAudioGraphView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._onGraphNodeClick = this._onGraphNodeClick.bind(this);
    this.draw = debounce(this.draw.bind(this), GRAPH_DEBOUNCE_TIMER);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    if (this._zoomBinding) {
      this._zoomBinding.on("zoom", null);
    }
  },

  /**
   * Called when a page is reloaded and waiting for a "start-context" event
   * and clears out old content
   */
  resetUI: function () {
    $("#reload-notice").hidden = true;
    $("#waiting-notice").hidden = false;
    $("#content").hidden = true;
    this.resetGraph();
  },

  /**
   * Called once "start-context" is fired, indicating that there is audio context
   * activity to view and inspect
   */
  showContent: function () {
    $("#reload-notice").hidden = true;
    $("#waiting-notice").hidden = true;
    $("#content").hidden = false;
    this.draw();
  },

  /**
   * Clears out the rendered graph, called when resetting the SVG elements to draw again,
   * or when resetting the entire UI tool
   */
  resetGraph: function () {
    $("#graph-target").innerHTML = "";
  },

  /**
   * Makes the corresponding graph node appear "focused", called from WebAudioParamView
   */
  focusNode: function (actorID) {
    // Remove class "selected" from all nodes
    Array.prototype.forEach.call($$(".nodes > g"), $node => $node.classList.remove("selected"));
    // Add to "selected"
    this._getNodeByID(actorID).classList.add("selected");
  },

  /**
   * Unfocuses the corresponding graph node, called from WebAudioParamView
   */
  blurNode: function (actorID) {
    this._getNodeByID(actorID).classList.remove("selected");
  },

  /**
   * Takes an actorID and returns the corresponding DOM SVG element in the graph
   */
  _getNodeByID: function (actorID) {
    return $(".nodes > g[data-id='" + actorID + "']");
  },

  /**
   * `draw` renders the ViewNodes currently available in `AudioNodes` with `AudioNodeConnections`,
   * and is throttled to be called at most every `GRAPH_DEBOUNCE_TIMER` milliseconds. Is called
   * whenever the audio context routing changes, after being debounced.
   */
  draw: function () {
    // Clear out previous SVG information
    this.resetGraph();

    let graph = new dagreD3.Digraph();
    let edges = [];

    AudioNodes.forEach(node => {
      // Add node to graph
      graph.addNode(node.id, { label: node.type, id: node.id });

      // Add all of the connections from this node to the edge array to be added
      // after all the nodes are added, otherwise edges will attempted to be created
      // for nodes that have not yet been added
      AudioNodeConnections.get(node, []).forEach(dest => edges.push([node, dest]));
    });

    edges.forEach(([node, dest]) => graph.addEdge(null, node.id, dest.id, {
      source: node.id,
      target: dest.id
    }));

    let renderer = new dagreD3.Renderer();

    // Post-render manipulation of the nodes
    let oldDrawNodes = renderer.drawNodes();
    renderer.drawNodes(function(graph, root) {
      let svgNodes = oldDrawNodes(graph, root);
      svgNodes.attr("class", (n) => {
        let node = graph.node(n);
        return "type-" + node.label;
      });
      svgNodes.attr("data-id", (n) => {
        let node = graph.node(n);
        return node.id;
      });
      return svgNodes;
    });

    // Post-render manipulation of edges
    let oldDrawEdgePaths = renderer.drawEdgePaths();
    renderer.drawEdgePaths(function(graph, root) {
      let svgNodes = oldDrawEdgePaths(graph, root);
      svgNodes.attr("data-source", (n) => {
        let edge = graph.edge(n);
        return edge.source;
      });
      svgNodes.attr("data-target", (n) => {
        let edge = graph.edge(n);
        return edge.target;
      });
      return svgNodes;
    });

    // Override Dagre-d3's post render function by passing in our own.
    // This way we can leave styles out of it.
    renderer.postRender(function (graph, root) {
      // TODO change arrowhead color depending on theme-dark/theme-light
      // and possibly refactor rendering this as it's ugly
      // Bug 994256
      // let color = window.classList.contains("theme-dark") ? "#f5f7fa" : "#585959";
      if (graph.isDirected() && root.select("#arrowhead").empty()) {
        root
          .append("svg:defs")
          .append("svg:marker")
          .attr("id", "arrowhead")
          .attr("viewBox", "0 0 10 10")
          .attr("refX", ARROW_WIDTH)
          .attr("refY", ARROW_HEIGHT)
          .attr("markerUnits", "strokewidth")
          .attr("markerWidth", ARROW_WIDTH)
          .attr("markerHeight", ARROW_HEIGHT)
          .attr("orient", "auto")
          .attr("style", "fill: #f5f7fa")
          .append("svg:path")
          .attr("d", "M 0 0 L 10 5 L 0 10 z");
      }

      // Fire an event upon completed rendering
      window.emit(EVENTS.UI_GRAPH_RENDERED, AudioNodes.length, edges.length);
    });

    let layout = dagreD3.layout().rankDir("LR");
    renderer.layout(layout).run(graph, d3.select("#graph-target"));

    // Handle the sliding and zooming of the graph,
    // store as `this._zoomBinding` so we can unbind during destruction
    if (!this._zoomBinding) {
      this._zoomBinding = d3.behavior.zoom().on("zoom", function () {
        var ev = d3.event;
        d3.select("#graph-target")
          .attr("transform", "translate(" + ev.translate + ") scale(" + ev.scale + ")");
      });
      d3.select("svg").call(this._zoomBinding);
    }
  },

  /**
   * Event handlers
   */

  /**
   * Fired when a node in the svg graph is clicked. Used to handle triggering the AudioNodePane.
   *
   * @param Object AudioNodeView
   *        The object stored in `AudioNodes` which contains render information, but most importantly,
   *        the actorID under `id` property.
   */
  _onGraphNodeClick: function (node) {
    WebAudioParamView.focusNode(node.id);
  }
};

let WebAudioParamView = {
  _paramsView: null,

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function () {
    this._paramsView = new VariablesView($("#web-audio-inspector-content"), GENERIC_VARIABLES_VIEW_SETTINGS);
    this._paramsView.eval = this._onEval.bind(this);
    window.on(EVENTS.CREATE_NODE, this.addNode = this.addNode.bind(this));
    window.on(EVENTS.DESTROY_NODE, this.removeNode = this.removeNode.bind(this));
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function() {
    window.off(EVENTS.CREATE_NODE, this.addNode);
    window.off(EVENTS.DESTROY_NODE, this.removeNode);
  },

  /**
   * Empties out the params view.
   */
  resetUI: function () {
    this._paramsView.empty();
  },

  /**
   * Takes an `id` and focuses and expands the corresponding scope.
   */
  focusNode: function (id) {
    let scope = this._getScopeByID(id);
    if (!scope) return;

    scope.focus();
    scope.expand();
  },

  /**
   * Executed when an audio param is changed in the UI.
   */
  _onEval: Task.async(function* (variable, value) {
    let ownerScope = variable.ownerView;
    let node = getViewNodeById(ownerScope.actorID);
    let propName = variable.name;
    let errorMessage = yield node.actor.setParam(propName, value);

    // TODO figure out how to handle and display set param errors
    // and enable `test/brorwser_wa_params_view_edit_error.js`
    // Bug 994258
    if (!errorMessage) {
      ownerScope.get(propName).setGrip(value);
      window.emit(EVENTS.UI_SET_PARAM, node.id, propName, value);
    } else {
      window.emit(EVENTS.UI_SET_PARAM_ERROR, node.id, propName, value);
    }
  }),

  /**
   * Takes an `id` and returns the corresponding variables scope.
   */
  _getScopeByID: function (id) {
    let view = this._paramsView;
    for (let i = 0; i < view._store.length; i++) {
      let scope = view.getScopeAtIndex(i);
      if (scope.actorID === id)
        return scope;
    }
    return null;
  },

  /**
   * Called when hovering over a variable scope.
   */
  _onMouseOver: function (e) {
    let id = WebAudioParamView._getScopeID(this);

    if (!id) return;

    WebAudioGraphView.focusNode(id);
  },

  /**
   * Called when hovering out of a variable scope.
   */
  _onMouseOut: function (e) {
    let id = WebAudioParamView._getScopeID(this);

    if (!id) return;

    WebAudioGraphView.blurNode(id);
  },

  /**
   * Uses in event handlers, takes an element `$el` and finds the
   * associated actor ID with that variable scope to be used in other contexts.
   */
  _getScopeID: function ($el) {
    let match = $el.parentNode.id.match(/\(([^\)]*)\)/);
    return match ? match[1] : null;
  },

  /**
   * Called when `CREATE_NODE` is fired to update the params view with the
   * freshly created audio node.
   */
  addNode: Task.async(function* (_, id) {
    let viewNode = getViewNodeById(id);
    let type = viewNode.type;

    let audioParamsTitle = type + " (" + id + ")";
    let paramsView = this._paramsView;
    let paramsScopeView = paramsView.addScope(audioParamsTitle);

    paramsScopeView.actorID = id;
    paramsScopeView.expanded = false;

    paramsScopeView.addEventListener("mouseover", this._onMouseOver, false);
    paramsScopeView.addEventListener("mouseout", this._onMouseOut, false);

    let params = yield viewNode.getParams();
    params.forEach(({ param, value }) => {
      let descriptor = { value: value };
      paramsScopeView.addItem(param, descriptor);
    });

    window.emit(EVENTS.UI_ADD_NODE_LIST, id);
  }),

  /**
   * Called when `DESTROY_NODE` is fired to remove the node from params view.
   * TODO bug 994263, dependent on node GC events
   */
  removeNode: Task.async(function* (viewNode) {

  })
};
