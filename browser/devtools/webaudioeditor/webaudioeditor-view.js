/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource:///modules/devtools/VariablesView.jsm");
Cu.import("resource:///modules/devtools/VariablesViewController.jsm");
const { debounce } = require("sdk/lang/functional");

// Strings for rendering
const EXPAND_INSPECTOR_STRING = L10N.getStr("expandInspector");
const COLLAPSE_INSPECTOR_STRING = L10N.getStr("collapseInspector");

// Store width as a preference rather than hardcode
// TODO bug 1009056
const INSPECTOR_WIDTH = 300;

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

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  searchEnabled: false,
  editableValueTooltip: "",
  editableNameTooltip: "",
  preventDisableOnChange: true,
  preventDescriptorModifiers: false,
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
    this._onThemeChange = this._onThemeChange.bind(this);
    this._onNodeSelect = this._onNodeSelect.bind(this);
    this._onStartContext = this._onStartContext.bind(this);
    this._onDestroyNode = this._onDestroyNode.bind(this);

    this.draw = debounce(this.draw.bind(this), GRAPH_DEBOUNCE_TIMER);
    $('#graph-target').addEventListener('click', this._onGraphNodeClick, false);

    window.on(EVENTS.THEME_CHANGE, this._onThemeChange);
    window.on(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSelect);
    window.on(EVENTS.START_CONTEXT, this._onStartContext);
    window.on(EVENTS.DESTROY_NODE, this._onDestroyNode);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    if (this._zoomBinding) {
      this._zoomBinding.on("zoom", null);
    }
    $('#graph-target').removeEventListener('click', this._onGraphNodeClick, false);
    window.off(EVENTS.THEME_CHANGE, this._onThemeChange);
    window.off(EVENTS.UI_INSPECTOR_NODE_SET, this._onNodeSelect);
    window.off(EVENTS.START_CONTEXT, this._onStartContext);
    window.off(EVENTS.DESTROY_NODE, this._onDestroyNode);
  },

  /**
   * Called when a page is reloaded and waiting for a "start-context" event
   * and clears out old content
   */
  resetUI: function () {
    this.clearGraph();
    this.resetGraphPosition();
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
  resetGraphPosition: function () {
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
   * Called from UI_INSPECTOR_NODE_SELECT.
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
   * `draw` renders the ViewNodes currently available in `AudioNodes` with `AudioNodeConnections`,
   * and is throttled to be called at most every `GRAPH_DEBOUNCE_TIMER` milliseconds. Is called
   * whenever the audio context routing changes, after being debounced.
   */
  draw: function () {
    // Clear out previous SVG information
    this.clearGraph();

    let graph = new dagreD3.Digraph();
    let edges = [];

    AudioNodes.forEach(node => {
      // Add node to graph
      graph.addNode(node.id, { label: node.type, id: node.id });

      // Add all of the connections from this node to the edge array to be added
      // after all the nodes are added, otherwise edges will attempted to be created
      // for nodes that have not yet been added
      AudioNodeConnections.get(node, new Set()).forEach(dest => edges.push([node, dest]));
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
        return "audionode type-" + node.label;
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
      let currentNode = WebAudioInspectorView.getCurrentAudioNode();
      if (currentNode) {
        this.focusNode(currentNode.id);
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

      // Set initial translation and scale -- this puts D3's awareness of
      // the graph in sync with what the user sees originally.
      this.resetGraphPosition();
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
   * Called when a node gets GC'd -- redraws the graph.
   */
  _onDestroyNode: function () {
    this.draw();
  },

  _onNodeSelect: function (eventName, id) {
    this.focusNode(id);
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

    window.emit(EVENTS.UI_SELECT_NODE, node.getAttribute("data-id"));
  }
};

let WebAudioInspectorView = {

  _propsView: null,

  _currentNode: null,

  _inspectorPane: null,
  _inspectorPaneToggleButton: null,
  _tabsPane: null,

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function () {
    this._inspectorPane = $("#web-audio-inspector");
    this._inspectorPaneToggleButton = $("#inspector-pane-toggle");
    this._tabsPane = $("#web-audio-editor-tabs");

    // Hide inspector view on startup
    this._inspectorPane.setAttribute("width", INSPECTOR_WIDTH);
    this.toggleInspector({ visible: false, delayed: false, animated: false });

    this._onEval = this._onEval.bind(this);
    this._onNodeSelect = this._onNodeSelect.bind(this);
    this._onTogglePaneClick = this._onTogglePaneClick.bind(this);
    this._onDestroyNode = this._onDestroyNode.bind(this);

    this._inspectorPaneToggleButton.addEventListener("mousedown", this._onTogglePaneClick, false);
    this._propsView = new VariablesView($("#properties-tabpanel-content"), GENERIC_VARIABLES_VIEW_SETTINGS);
    this._propsView.eval = this._onEval;

    window.on(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    window.on(EVENTS.DESTROY_NODE, this._onDestroyNode);
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function () {
    this._inspectorPaneToggleButton.removeEventListener("mousedown", this._onTogglePaneClick);
    window.off(EVENTS.UI_SELECT_NODE, this._onNodeSelect);
    window.off(EVENTS.DESTROY_NODE, this._onDestroyNode);

    this._inspectorPane = null;
    this._inspectorPaneToggleButton = null;
    this._tabsPane = null;
  },

  /**
   * Toggles the visibility of the AudioNode Inspector.
   *
   * @param object visible
   *        - visible: boolean indicating whether the panel should be shown or not
   *        - animated: boolean indiciating whether the pane should be animated
   *        - delayed: boolean indicating whether the pane's opening should wait
   *                   a few cycles or not
   *        - index: the index of the tab to be selected inside the inspector
   * @param number index
   *        Index of the tab that should be selected when shown.
   */
  toggleInspector: function ({ visible, animated, delayed, index }) {
    let pane = this._inspectorPane;
    let button = this._inspectorPaneToggleButton;

    let flags = {
      visible: visible,
      animated: animated != null ? animated : true,
      delayed: delayed != null ? delayed : true,
      callback: () => window.emit(EVENTS.UI_INSPECTOR_TOGGLED, visible)
    };

    ViewHelpers.togglePane(flags, pane);

    if (flags.visible) {
      button.removeAttribute("pane-collapsed");
      button.setAttribute("tooltiptext", COLLAPSE_INSPECTOR_STRING);
    }
    else {
      button.setAttribute("pane-collapsed", "");
      button.setAttribute("tooltiptext", EXPAND_INSPECTOR_STRING);
    }

    if (index != undefined) {
      pane.selectedIndex = index;
    }
  },

  /**
   * Returns a boolean indicating whether or not the AudioNode inspector
   * is currently being shown.
   */
  isVisible: function () {
    return !this._inspectorPane.hasAttribute("pane-collapsed");
  },

  /**
   * Takes a AudioNodeView `node` and sets it as the current
   * node and scaffolds the inspector view based off of the new node.
   */
  setCurrentAudioNode: function (node) {
    this._currentNode = node || null;

    // If no node selected, set the inspector back to "no AudioNode selected"
    // view.
    if (!node) {
      $("#web-audio-editor-details-pane-empty").removeAttribute("hidden");
      $("#web-audio-editor-tabs").setAttribute("hidden", "true");
      window.emit(EVENTS.UI_INSPECTOR_NODE_SET, null);
    }
    // Otherwise load up the tabs view and hide the empty placeholder
    else {
      $("#web-audio-editor-details-pane-empty").setAttribute("hidden", "true");
      $("#web-audio-editor-tabs").removeAttribute("hidden");
      this._setTitle();
      this._buildPropertiesView()
        .then(() => window.emit(EVENTS.UI_INSPECTOR_NODE_SET, this._currentNode.id));
    }
  },

  /**
   * Returns the current AudioNodeView.
   */
  getCurrentAudioNode: function () {
    return this._currentNode;
  },

  /**
   * Empties out the props view.
   */
  resetUI: function () {
    this._propsView.empty();
    // Set current node to empty to load empty view
    this.setCurrentAudioNode();

    // Reset AudioNode inspector and hide
    this.toggleInspector({ visible: false, animated: false, delayed: false });
  },

  /**
   * Sets the title of the Inspector view
   */
  _setTitle: function () {
    let node = this._currentNode;
    let title = node.type + " (" + node.id + ")";
    $("#web-audio-inspector-title").setAttribute("value", title);
  },

  /**
   * Reconstructs the `Properties` tab in the inspector
   * with the `this._currentNode` as it's source.
   */
  _buildPropertiesView: Task.async(function* () {
    let propsView = this._propsView;
    let node = this._currentNode;
    propsView.empty();

    let audioParamsScope = propsView.addScope("AudioParams");
    let props = yield node.getParams();

    // Disable AudioParams VariableView expansion
    // when there are no props i.e. AudioDestinationNode
    this._togglePropertiesView(!!props.length);

    props.forEach(({ param, value, flags }) => {
      let descriptor = {
        value: value,
        writable: !flags || !flags.readonly,
      };
      audioParamsScope.addItem(param, descriptor);
    });

    audioParamsScope.expanded = true;

    window.emit(EVENTS.UI_PROPERTIES_TAB_RENDERED, node.id);
  }),

  _togglePropertiesView: function (show) {
    let propsView = $("#properties-tabpanel-content");
    let emptyView = $("#properties-tabpanel-content-empty");
    (show ? propsView : emptyView).removeAttribute("hidden");
    (show ? emptyView : propsView).setAttribute("hidden", "true");
  },

  /**
   * Returns the scope for AudioParams in the
   * VariablesView.
   *
   * @return Scope
   */
  _getAudioPropertiesScope: function () {
    return this._propsView.getScopeAtIndex(0);
  },

  /**
   * Event handlers
   */

  /**
   * Executed when an audio prop is changed in the UI.
   */
  _onEval: Task.async(function* (variable, value) {
    let ownerScope = variable.ownerView;
    let node = this._currentNode;
    let propName = variable.name;
    let error;

    if (!variable._initialDescriptor.writable) {
      error = new Error("Variable " + propName + " is not writable.");
    } else {
      // Cast value to proper type
      try {
        let number = parseFloat(value);
        if (!isNaN(number)) {
          value = number;
        } else {
          value = JSON.parse(value);
        }
        error = yield node.actor.setParam(propName, value);
      }
      catch (e) {
        error = e;
      }
    }

    // TODO figure out how to handle and display set prop errors
    // and enable `test/brorwser_wa_properties-view-edit.js`
    // Bug 994258
    if (!error) {
      ownerScope.get(propName).setGrip(value);
      window.emit(EVENTS.UI_SET_PARAM, node.id, propName, value);
    } else {
      window.emit(EVENTS.UI_SET_PARAM_ERROR, node.id, propName, value);
    }
  }),

  /**
   * Called on EVENTS.UI_SELECT_NODE, and takes an actorID `id`
   * and calls `setCurrentAudioNode` to scaffold the inspector view.
   */
  _onNodeSelect: function (_, id) {
    this.setCurrentAudioNode(getViewNodeById(id));

    // Ensure inspector is visible when selecting a new node
    this.toggleInspector({ visible: true });
  },

  /**
   * Called when clicking on the toggling the inspector into view.
   */
  _onTogglePaneClick: function () {
    this.toggleInspector({ visible: !this.isVisible() });
  },

  /**
   * Called when `DESTROY_NODE` is fired to remove the node from props view if
   * it's currently selected.
   */
  _onDestroyNode: function (_, id) {
    if (this._currentNode && this._currentNode.id === id) {
      this.setCurrentAudioNode(null);
    }
  }
};

/**
 * Takes an element in an SVG graph and iterates over
 * ancestors until it finds the graph node container. If not found,
 * returns null.
 */

function findGraphNodeParent (el) {
  // Some targets may not contain `classList` property
  if (!el.classList)
    return null;

  while (!el.classList.contains("nodes")) {
    if (el.classList.contains("audionode"))
      return el;
    else
      el = el.parentNode;
  }
  return null;
}
