/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { debounce } = require("sdk/lang/functional");

// Globals for d3 stuff
// Default properties of the graph on rerender
const GRAPH_DEFAULTS = {
  translate: [20, 20],
  scale: 1
};

// Sizes of SVG arrows in graph
const ARROW_HEIGHT = 5;
const ARROW_WIDTH = 8;

// Styles for markers as they cannot be done with CSS.
const MARKER_STYLING = {
  light: "#AAA",
  dark: "#CED3D9"
};

const GRAPH_DEBOUNCE_TIMER = 100;

// `gAudioNodes` events that should require the graph
// to redraw
const GRAPH_REDRAW_EVENTS = ["add", "connect", "disconnect", "remove"];

/**
 * Functions handling the graph UI.
 */
let ContextView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._onGraphNodeClick = this._onGraphNodeClick.bind(this);
    this._onThemeChange = this._onThemeChange.bind(this);
    this._onStartContext = this._onStartContext.bind(this);
    this._onEvent = this._onEvent.bind(this);

    this.draw = debounce(this.draw.bind(this), GRAPH_DEBOUNCE_TIMER);
    $('#graph-target').addEventListener('click', this._onGraphNodeClick, false);

    window.on(EVENTS.THEME_CHANGE, this._onThemeChange);
    window.on(EVENTS.START_CONTEXT, this._onStartContext);
    gAudioNodes.on("*", this._onEvent);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    // If the graph was rendered at all, then the handler
    // for zooming in will be set. We must remove it to prevent leaks.
    if (this._zoomBinding) {
      this._zoomBinding.on("zoom", null);
    }
    $('#graph-target').removeEventListener('click', this._onGraphNodeClick, false);
    window.off(EVENTS.THEME_CHANGE, this._onThemeChange);
    window.off(EVENTS.START_CONTEXT, this._onStartContext);
    gAudioNodes.off("*", this._onEvent);
  },

  /**
   * Called when a page is reloaded and waiting for a "start-context" event
   * and clears out old content
   */
  resetUI: function () {
    this.clearGraph();
    this.resetGraphTransform();
  },

  /**
   * Clears out the rendered graph, called when resetting the SVG elements to draw again,
   * or when resetting the entire UI tool
   */
  clearGraph: function () {
    $("#graph-target").innerHTML = "";
  },

  /**
   * Moves the graph back to its original scale and translation.
   */
  resetGraphTransform: function () {
    // Only reset if the graph was ever drawn.
    if (this._zoomBinding) {
      let { translate, scale } = GRAPH_DEFAULTS;
      // Must set the `zoomBinding` so the next `zoom` event is in sync with
      // where the graph is visually (set by the `transform` attribute).
      this._zoomBinding.scale(scale);
      this._zoomBinding.translate(translate);
      d3.select("#graph-target")
        .attr("transform", "translate(" + translate + ") scale(" + scale + ")");
    }
  },

  getCurrentScale: function () {
    return this._zoomBinding ? this._zoomBinding.scale() : null;
  },

  getCurrentTranslation: function () {
    return this._zoomBinding ? this._zoomBinding.translate() : null;
  },

  /**
   * Makes the corresponding graph node appear "focused", removing
   * focused styles from all other nodes. If no `actorID` specified,
   * make all nodes appear unselected.
   */
  focusNode: function (actorID) {
    // Remove class "selected" from all nodes
    Array.forEach($$(".nodes > g"), $node => $node.classList.remove("selected"));
    // Add to "selected"
    if (actorID) {
      this._getNodeByID(actorID).classList.add("selected");
    }
  },

  /**
   * Takes an actorID and returns the corresponding DOM SVG element in the graph
   */
  _getNodeByID: function (actorID) {
    return $(".nodes > g[data-id='" + actorID + "']");
  },

  /**
   * This method renders the nodes currently available in `gAudioNodes` and is
   * throttled to be called at most every `GRAPH_DEBOUNCE_TIMER` milliseconds.
   * It's called whenever the audio context routing changes, after being debounced.
   */
  draw: function () {
    // Clear out previous SVG information
    this.clearGraph();

    let graph = new dagreD3.Digraph();
    let renderer = new dagreD3.Renderer();
    gAudioNodes.populateGraph(graph);

    // Post-render manipulation of the nodes
    let oldDrawNodes = renderer.drawNodes();
    renderer.drawNodes(function(graph, root) {
      let svgNodes = oldDrawNodes(graph, root);
      svgNodes.attr("class", (n) => {
        let node = graph.node(n);
        return "audionode type-" + node.type;
      });
      svgNodes.attr("data-id", (n) => {
        let node = graph.node(n);
        return node.id;
      });
      return svgNodes;
    });

    // Post-render manipulation of edges
    // TODO do all of this more efficiently, rather than
    // using the direct D3 helper utilities to loop over each
    // edge several times
    let oldDrawEdgePaths = renderer.drawEdgePaths();
    renderer.drawEdgePaths(function(graph, root) {
      let svgEdges = oldDrawEdgePaths(graph, root);
      svgEdges.attr("data-source", (n) => {
        let edge = graph.edge(n);
        return edge.source;
      });
      svgEdges.attr("data-target", (n) => {
        let edge = graph.edge(n);
        return edge.target;
      });
      svgEdges.attr("data-param", (n) => {
        let edge = graph.edge(n);
        return edge.param ? edge.param : null;
      });
      // We have to manually specify the default classes on the edges
      // as to not overwrite them
      let defaultClasses = "edgePath enter";
      svgEdges.attr("class", (n) => {
        let edge = graph.edge(n);
        return defaultClasses + (edge.param ? (" param-connection " + edge.param) : "");
      });

      return svgEdges;
    });

    // Override Dagre-d3's post render function by passing in our own.
    // This way we can leave styles out of it.
    renderer.postRender((graph, root) => {
      // We have to manually set the marker styling since we cannot
      // do this currently with CSS, although it is in spec for SVG2
      // https://svgwg.org/svg2-draft/painting.html#VertexMarkerProperties
      // For now, manually set it on creation, and the `_onThemeChange`
      // function will fire when the devtools theme changes to update the
      // styling manually.
      let theme = Services.prefs.getCharPref("devtools.theme");
      let markerColor = MARKER_STYLING[theme];
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
          .attr("style", "fill: " + markerColor)
          .append("svg:path")
          .attr("d", "M 0 0 L 10 5 L 0 10 z");
      }

      // Reselect the previously selected audio node
      let currentNode = InspectorView.getCurrentAudioNode();
      if (currentNode) {
        this.focusNode(currentNode.id);
      }

      // Fire an event upon completed rendering, with extra information
      // if in testing mode only.
      let info = {};
      if (gDevTools.testing) {
        info = gAudioNodes.getInfo();
      }
      window.emit(EVENTS.UI_GRAPH_RENDERED, info.nodes, info.edges, info.paramEdges);
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

      // Set initial translation and scale -- this puts D3's awareness of
      // the graph in sync with what the user sees originally.
      this.resetGraphTransform();
    }
  },

  /**
   * Event handlers
   */

  /**
   * Called once "start-context" is fired, indicating that there is an audio
   * context being created to view so render the graph.
   */
  _onStartContext: function () {
    this.draw();
  },

  /**
   * Called when `gAudioNodes` fires an event -- most events (listed
   * in GRAPH_REDRAW_EVENTS) qualify as a redraw event.
   */
  _onEvent: function (eventName, ...args) {
    if (~GRAPH_REDRAW_EVENTS.indexOf(eventName)) {
      this.draw();
    }
  },

  /**
   * Fired when the devtools theme changes.
   */
  _onThemeChange: function (eventName, theme) {
    let markerColor = MARKER_STYLING[theme];
    let marker = $("#arrowhead");
    if (marker) {
      marker.setAttribute("style", "fill: " + markerColor);
    }
  },

  /**
   * Fired when a node in the svg graph is clicked. Used to handle triggering the AudioNodePane.
   *
   * @param Event e
   *        Click event.
   */
  _onGraphNodeClick: function (e) {
    let node = findGraphNodeParent(e.target);
    // If node not found (clicking outside of an audio node in the graph),
    // then ignore this event
    if (!node)
      return;

    let id = node.getAttribute("data-id");

    this.focusNode(id);
    window.emit(EVENTS.UI_SELECT_NODE, id);
  }
};
