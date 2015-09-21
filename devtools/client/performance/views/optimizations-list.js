/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const URL_LABEL_TOOLTIP = L10N.getStr("table.url.tooltiptext");
const OPTIMIZATION_FAILURE = L10N.getStr("jit.optimizationFailure");
const JIT_SAMPLES = L10N.getStr("jit.samples");
const JIT_EMPTY_TEXT = L10N.getStr("jit.empty");
const PROPNAME_MAX_LENGTH = 4;

/**
 * View for rendering a list of all optmizations found in a frame.
 * The terminology and types used here can be referenced:
 * @see devtools/client/performance/modules/logic/jit.js
 */

var OptimizationsListView = {

  _currentFrame: null,

  /**
   * Initialization function called when the tool starts up.
   */
  initialize: function () {
    this.reset = this.reset.bind(this);
    this._onThemeChanged = this._onThemeChanged.bind(this);

    this.el = $("#jit-optimizations-view");
    this.$headerName = $("#jit-optimizations-header .header-function-name");
    this.$headerFile = $("#jit-optimizations-header .header-file");
    this.$headerLine = $("#jit-optimizations-header .header-line");

    this.tree = new TreeWidget($("#jit-optimizations-raw-view"), {
      sorted: false,
      emptyText: JIT_EMPTY_TEXT
    });
    this.graph = new OptimizationsGraph($("#optimizations-graph"));
    this.graph.setTheme(PerformanceController.getTheme());

    // Start the tree by resetting.
    this.reset();

    PerformanceController.on(EVENTS.THEME_CHANGED, this._onThemeChanged);
  },

  /**
   * Destruction function called when the tool cleans up.
   */
  destroy: function () {
    PerformanceController.off(EVENTS.THEME_CHANGED, this._onThemeChanged);
    this.tree = null;
    this.$headerName = this.$headerFile = this.$headerLine = this.el = null;
  },

  /**
   * Takes a FrameNode, with corresponding optimization data to be displayed
   * in the view.
   *
   * @param {FrameNode} frameNode
   */
  setCurrentFrame: function (threadNode, frameNode) {
    if (threadNode !== this.getCurrentThread()) {
      this._currentThread = threadNode;
    }
    if (frameNode !== this.getCurrentFrame()) {
      this._currentFrame = frameNode;
    }
  },

  /**
   * Returns the current frame node for this view.
   *
   * @return {?FrameNode}
   */
  getCurrentFrame: function () {
    return this._currentFrame;
  },

  /**
   * Returns the current thread node for this view.
   *
   * @return {?ThreadNode}
   */
  getCurrentThread: function () {
    return this._currentThread;
  },

  /**
   * Clears out data in the tree, sets to an empty state,
   * and removes current frame.
   */
  reset: function () {
    this.setCurrentFrame(null, null);
    this.clear();
    this.el.classList.add("empty");
    this.emit(EVENTS.OPTIMIZATIONS_RESET);
    this.emit(EVENTS.OPTIMIZATIONS_RENDERED, this.getCurrentFrame());
  },

  /**
   * Clears out data in the tree.
   */
  clear: function () {
    this.tree.clear();
  },

  /**
   * Takes a JITOptimizations object and builds a view containing all attempted
   * optimizations for this frame. This view is very verbose and meant for those
   * who understand JIT compilers.
   */
  render: function () {
    let frameNode = this.getCurrentFrame();

    if (!frameNode) {
      this.reset();
      return;
    }

    let view = this.tree;

    // Set header information, even if the frame node
    // does not have any optimization data
    let frameData = frameNode.getInfo();
    this._setHeaders(frameData);
    this.clear();

    // If this frame node does not have optimizations, or if its a meta node in the
    // case of only showing content, reset the view.
    if (!frameNode.hasOptimizations() || frameNode.isMetaCategory) {
      this.reset();
      return;
    }
    this.el.classList.remove("empty");

    // An array of sorted OptimizationSites.
    let sites = frameNode.getOptimizations().optimizationSites;

    for (let site of sites) {
      this._renderSite(view, site, frameData);
    }

    this._renderTierGraph();

    this.emit(EVENTS.OPTIMIZATIONS_RENDERED, this.getCurrentFrame());
  },

  /**
   * Renders the optimization tier graph over time.
   */
  _renderTierGraph: function () {
    this.graph.render(this.getCurrentThread(), this.getCurrentFrame());
  },

  /**
   * Creates an entry in the tree widget for an optimization site.
   */
  _renderSite: function (view, site, frameData) {
    let { id, samples, data } = site;
    let { types, attempts } = data;
    let siteNode = this._createSiteNode(frameData, site);

    // Cast `id` to a string so TreeWidget doesn't think it does not exist
    id = id + "";

    view.add([{ id: id, node: siteNode }]);

    // Add types -- Ion types are the parent, with
    // the observed types as children.
    view.add([id, { id: `${id}-types`, label: `Types (${types.length})` }]);
    this._renderIonType(view, site);

    // Add attempts
    view.add([id, { id: `${id}-attempts`, label: `Attempts (${attempts.length})` }]);
    for (let i = attempts.length - 1; i >= 0; i--) {
      let node = this._createAttemptNode(attempts[i]);
      view.add([id, `${id}-attempts`, { node }]);
    }
  },

  /**
   * Renders all Ion types from an optimization site, with its children
   * ObservedTypes.
   */
  _renderIonType: function (view, site) {
    let { id, data: { types }} = site;
    // Cast `id` to a string so TreeWidget doesn't think it does not exist
    id = id + "";
    for (let i = 0; i < types.length; i++) {
      let ionType = types[i];

      let ionNode = this._createIonNode(ionType);
      view.add([id, `${id}-types`, { id: `${id}-types-${i}`, node: ionNode }]);
      for (let observedType of (ionType.typeset || [])) {
        let node = this._createObservedTypeNode(observedType);
        view.add([id, `${id}-types`, `${id}-types-${i}`, { node }]);
      }
    }
  },

  /**
   * Creates an element for insertion in the raw view for an OptimizationSite.
   */
  _createSiteNode: function (frameData, site) {
    let node = document.createElement("span");
    let desc = document.createElement("span");
    let line = document.createElement("span");
    let column = document.createElement("span");
    let urlNode = this._createDebuggerLinkNode(frameData.url, site.data.line);

    let attempts = site.getAttempts();
    let lastStrategy = attempts[attempts.length - 1].strategy;

    let propString = "";
    if (site.data.propertyName) {
      if (site.data.propertyName.length > PROPNAME_MAX_LENGTH) {
        propString = ` (.${site.data.propertyName.substr(0, PROPNAME_MAX_LENGTH)}…)`;
        desc.setAttribute("tooltiptext", site.data.propertyName);
      } else {
        propString = ` (.${site.data.propertyName})`;
      }
    }

    if (!site.hasSuccessfulOutcome()) {
      let icon = document.createElement("span");
      icon.setAttribute("tooltiptext", OPTIMIZATION_FAILURE);
      icon.setAttribute("severity", "warning");
      icon.className = "opt-icon";
      node.appendChild(icon);
    }

    let sampleString = PluralForm.get(site.samples, JIT_SAMPLES).replace("#1", site.samples);
    desc.textContent = `${lastStrategy}${propString} – (${sampleString})`;
    line.textContent = site.data.line;
    line.className = "opt-line";
    column.textContent = site.data.column;
    column.className = "opt-line";
    node.appendChild(desc);
    node.appendChild(urlNode);
    node.appendChild(line);
    node.appendChild(column);

    return node;
  },

  /**
   * Creates an element for insertion in the raw view for an IonType.
   *
   * @see devtools/client/performance/modules/logic/jit.js
   * @param {IonType} ionType
   * @return {Element}
   */
  _createIonNode: function (ionType) {
    let node = document.createElement("span");
    node.textContent = `${ionType.site} : ${ionType.mirType}`;
    node.className = "opt-ion-type";
    return node;
  },

  /**
   * Creates an element for insertion in the raw view for an ObservedType.
   *
   * @see devtools/client/performance/modules/logic/jit.js
   * @param {ObservedType} type
   * @return {Element}
   */
  _createObservedTypeNode: function (type) {
    let node = document.createElement("span");
    let typeNode = document.createElement("span");

    typeNode.textContent = `${type.keyedBy}` + (type.name ? ` → ${type.name}` : "");
    typeNode.className = "opt-type";
    node.appendChild(typeNode);

    // If we have a type and a location, try to make a
    // link to the debugger
    if (type.location && type.line) {
      let urlNode = this._createDebuggerLinkNode(type.location, type.line);
      node.appendChild(urlNode);
    }
    // Otherwise if we just have a location, it could just
    // be a memory location
    else if (type.location) {
      let locNode = document.createElement("span");
      locNode.textContent = `@${type.location}`;
      locNode.className = "opt-url";
      node.appendChild(locNode);
    }

    if (type.line) {
      let line = document.createElement("span");
      line.textContent = type.line;
      line.className = "opt-line";
      node.appendChild(line);
    }
    return node;
  },

  /**
   * Creates an element for insertion in the raw view for an OptimizationAttempt.
   *
   * @see devtools/client/performance/modules/logic/jit.js
   * @param {OptimizationAttempt} attempt
   * @return {Element}
   */
  _createAttemptNode: function (attempt) {
    let node = document.createElement("span");
    let strategyNode = document.createElement("span");
    let outcomeNode = document.createElement("span");

    strategyNode.textContent = attempt.strategy;
    strategyNode.className = "opt-strategy";
    outcomeNode.textContent = attempt.outcome;
    outcomeNode.className = "opt-outcome";
    outcomeNode.setAttribute("outcome",
      JITOptimizations.isSuccessfulOutcome(attempt.outcome) ? "success" : "failure");

    node.appendChild(strategyNode);
    node.appendChild(outcomeNode);
    node.className = "opt-attempt";
    return node;
  },

  /**
   * Creates a new element, linking it up to the debugger upon clicking.
   * Can also optionally pass in an element to modify it rather than
   * creating a new one.
   *
   * @param {String} url
   * @param {Number} line
   * @param {?Element} el
   * @return {Element}
   */
  _createDebuggerLinkNode: function (url, line, el) {
    let node = el || document.createElement("span");
    node.className = "opt-url";
    let fileName;

    if (this._isLinkableURL(url)) {
      fileName = url.slice(url.lastIndexOf("/") + 1);
      node.classList.add("debugger-link");
      node.setAttribute("tooltiptext", URL_LABEL_TOOLTIP + " → " + url);
      node.addEventListener("click", () => gToolbox.viewSourceInDebugger(url, line));
    }
    fileName = fileName || url || "";
    node.textContent = fileName ? `@${fileName}` : "";
    return node;
  },

  /**
   * Updates the headers with the current frame's data.
   */
  _setHeaders: function (frameData) {
    let isMeta = frameData.isMetaCategory;
    let name = isMeta ? frameData.categoryData.label : frameData.functionName;
    let url = isMeta ? "" : frameData.url;
    let line = isMeta ? "" : frameData.line;

    this.$headerName.textContent = name;
    this.$headerLine.textContent = line;
    this._createDebuggerLinkNode(url, line, this.$headerFile);

    this.$headerLine.hidden = isMeta;
    this.$headerFile.hidden = isMeta;
  },

  /**
   * Takes a string and returns a boolean indicating whether or not
   * this is a valid url for linking to the debugger.
   *
   * @param {String} url
   * @return {Boolean}
   */
  _isLinkableURL: function (url) {
    return url && url.indexOf &&
       (url.indexOf("http") === 0 ||
        url.indexOf("resource://") === 0 ||
        url.indexOf("file://") === 0);
  },

  /**
   * Called when `devtools.theme` changes.
   */
  _onThemeChanged: function (_, theme) {
    this.graph.setTheme(theme);
    this.graph.refresh({ force: true });
  },

  toString: () => "[object OptimizationsListView]"
};

EventEmitter.decorate(OptimizationsListView);
