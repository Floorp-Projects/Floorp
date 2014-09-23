/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the profile inspection UI, showing the framerate and
 * cateogry graphs, along with a call tree view.
 *
 * A profile view is a tabbed browser, so recording data will be displayed in
 * tabs. Certain messages like 'Loading' or 'Recording...' may also be shown.
 */
let ProfileView = {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this._tabs = $("#profile-content tabs");
    this._panels = $("#profile-content tabpanels");
    this._tabTemplate = $("#profile-content-tab-template");
    this._panelTemplate = $("#profile-content-tabpanel-template");
    this._newtabButton = $("#profile-newtab-button");

    this._recordingInfoByPanel = new WeakMap();
    this._framerateGraphByPanel = new Map();
    this._categoriesGraphByPanel = new Map();
    this._callTreeRootByPanel = new Map();

    this._onTabSelect = this._onTabSelect.bind(this);
    this._onNewTabClick = this._onNewTabClick.bind(this);
    this._onGraphLegendSelection = this._onGraphLegendSelection.bind(this);
    this._onGraphMouseUp = this._onGraphMouseUp.bind(this);
    this._onGraphScroll = this._onGraphScroll.bind(this);
    this._onCallViewFocus = this._onCallViewFocus.bind(this);
    this._onCallViewLink = this._onCallViewLink.bind(this);
    this._onCallViewZoom = this._onCallViewZoom.bind(this);

    this._panels.addEventListener("select", this._onTabSelect, false);
    this._newtabButton.addEventListener("click", this._onNewTabClick, false);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.removeAllTabs();

    this._panels.removeEventListener("select", this._onTabSelect, false);
    this._newtabButton.removeEventListener("click", this._onNewTabClick, false);
  },

  /**
   * Shows a message detailing that there are is no data available.
   * The tabbed browser will also be hidden.
   */
  showEmptyNotice: function() {
    $("#profile-pane").selectedPanel = $("#empty-notice");
    window.emit(EVENTS.EMPTY_NOTICE_SHOWN);
  },

  /**
   * Shows a message detailing that a recording is currently in progress.
   * The tabbed browser will also be hidden.
   */
  showRecordingNotice: function() {
    $("#profile-pane").selectedPanel = $("#recording-notice");
    window.emit(EVENTS.RECORDING_NOTICE_SHOWN);
  },

  /**
   * Shows a message detailing that a finished recording is being loaded.
   * The tabbed browser will also be hidden.
   */
  showLoadingNotice: function() {
    $("#profile-pane").selectedPanel = $("#loading-notice");
    window.emit(EVENTS.LOADING_NOTICE_SHOWN);
  },

  /**
   * Shows the tabbed browser displaying recording data.
   */
  showTabbedBrowser: function() {
    $("#profile-pane").selectedPanel = $("#profile-content");
    window.emit(EVENTS.TABBED_BROWSER_SHOWN);
  },

  /**
   * Selects the tab at the specified index in this tabbed browser.
   *
   * @param number tabIndex
   *        The index of the tab to select. If no tab is available at the
   *        specified index, all tabs will be deselected.
   */
  selectTab: function(tabIndex) {
    $("#profile-content").selectedIndex = tabIndex;
  },

  /**
   * Adds an empty tab in this tabbed browser.
   *
   * @return number
   *         The newly created tab's index.
   */
  addTab: function() {
    let tab = this._tabs.appendChild(this._tabTemplate.cloneNode(true));
    let panel = this._panels.appendChild(this._panelTemplate.cloneNode(true));

    // "Uncover" the tab via a CSS animation.
    tab.removeAttribute("covered");

    let tabIndex = this._tabs.itemCount - 1;
    return tabIndex;
  },

  /**
   * Sets the title of a tab in this tabbed browser.
   *
   * @param number tabIndex
   *        The index of the tab to name.
   * @param number beginAt, endAt
   *        The 'start → stop' components of the tab title.
   */
  nameTab: function(tabIndex, beginAt, endAt) {
    let tab = this._getTab(tabIndex);
    let a = L10N.numberWithDecimals(beginAt, 2);
    let b = L10N.numberWithDecimals(endAt, 2);
    let labelNode = $(".tab-title-label", tab);
    labelNode.setAttribute("value", L10N.getFormatStr("profile.tab", a, b));
  },

  /**
   * Populates the panel for a tab in this tabbed browser with the provided
   * recording data.
   *
   * @param number tabIndex
   *        The index of the tab to populate.
   * @param object recordingData
   *        The profiler and refresh driver ticks data received from the front.
   * @param number beginAt
   *        The earliest time in the recording data to start at (in milliseconds).
   * @param number endAt
   *        The latest time in the recording data to end at (in milliseconds).
   * @param object options
   *        Additional options supported by this operation.
   *        @see ProfileView._populatePanelWidgets
   */
  populateTab: Task.async(function*(tabIndex, recordingData, beginAt, endAt, options) {
    let tab = this._getTab(tabIndex);
    let panel = this._getPanel(tabIndex);
    if (!tab || !panel) {
      return;
    }

    this._recordingInfoByPanel.set(panel, {
      recordingData: recordingData,
      displayRange: { beginAt: beginAt, endAt: endAt }
    });

    let { profilerData, ticksData } = recordingData;
    let categoriesData = RecordingUtils.plotCategoriesFor(profilerData, beginAt, endAt);
    let framerateData = RecordingUtils.plotFramerateFor(ticksData, beginAt, endAt);
    RecordingUtils.syncCategoriesWithFramerate(categoriesData, framerateData);

    yield this._populatePanelWidgets(panel, {
      profilerData: profilerData,
      framerateData: framerateData,
      categoriesData: categoriesData
    }, beginAt, endAt, options);
  }),

  /**
   * Adds a new tab in this tabbed browser, populates it with the provided
   * recording data and automatically selects it.
   *
   * @param object recordingData
   *        The profiler and refresh driver ticks data received from the front.
   * @param number beginAt
   *        The earliest time in the recording data to start at (in milliseconds).
   * @param number endAt
   *        The latest time in the recording data to end at (in milliseconds).
   * @param object options
   *        Additional options supported by this operation.
   *        @see ProfileView._populatePanelWidgets
   */
  addTabAndPopulate: Task.async(function*(recordingData, beginAt, endAt, options) {
    let tabIndex = this.addTab();
    this.nameTab(tabIndex, beginAt, endAt);

    // Wait for a few milliseconds before presenting the recording data,
    // to allow the 'Loading' panel to finish being drawn (if there is one).
    yield DevToolsUtils.waitForTime(RECORDING_DATA_DISPLAY_DELAY);
    yield this.populateTab(tabIndex, recordingData, beginAt, endAt, options);
    this.selectTab(tabIndex);
  }),

  /**
   * Removes all tabs and corresponding views from this tabbed browser.
   */
  removeAllTabs: function() {
    for (let [, graph] of this._framerateGraphByPanel) graph.destroy();
    for (let [, graph] of this._categoriesGraphByPanel) graph.destroy();
    for (let [, root] of this._callTreeRootByPanel) root.remove();

    this._recordingInfoByPanel = new WeakMap();
    this._framerateGraphByPanel.clear();
    this._categoriesGraphByPanel.clear();
    this._callTreeRootByPanel.clear();

    while (this._tabs.hasChildNodes()) {
      this._tabs.firstChild.remove();
    }
    while (this._panels.hasChildNodes()) {
      this._panels.firstChild.remove();
    }
  },

  /**
   * Removes all tabs exclusively after the one at the specified index.
   *
   * @param number tabIndex
   *        The "leftmost" tab to still keep. Remaining tabs will be removed.
   */
  removeTabsAfter: function(tabIndex) {
    tabIndex++;

    while (tabIndex < this._tabs.itemCount) {
      let tab = this._getTab(tabIndex);
      let panel = this._getPanel(tabIndex);

      this._framerateGraphByPanel.delete(panel);
      this._categoriesGraphByPanel.delete(panel);
      this._callTreeRootByPanel.delete(panel);
      tab.remove();
      panel.remove();
    }
  },

  /**
   * Gets the total number of tabs displayed in this tabbed browser.
   * @return number
   */
  get tabCount() {
    let tabs = this._tabs.childNodes.length;
    let tabpanels = this._panels.childNodes.length;
    if (tabs != tabpanels) {
      throw "The number of tabs isn't equal to the number of tabpanels.";
    }
    return tabs;
  },

  /**
   * Adds a new tab in this tabbed browser, populates it based on the current
   * selection range in the displayed data and automatically selects it.
   */
  _spawnTabFromSelection: Task.async(function*() {
    let { recordingData } = this._getRecordingInfo();
    let categoriesGraph = this._getCategoriesGraph();

    // A selection is assumed to be available in the current tab.
    let { min: beginAt, max: endAt } = categoriesGraph.getMappedSelection();

    // Hide the "new tab" button since a selection won't implicitly be made
    // in the newly created tab.
    this._newtabButton.hidden = true;

    yield this.addTabAndPopulate(recordingData, beginAt, endAt);

    // Signal that a new tab was spawned from a graph's selection.
    window.emit(EVENTS.TAB_SPAWNED_FROM_SELECTION);
  }),

  /**
   * Adds a new tab in this tabbed browser, populates it based on the provided
   * frame node and automatically selects it.
   *
   * @param FrameNode frameNode
   *        Information about the function call node in the tree.
   */
  _spawnTabFromFrameNode: Task.async(function*(frameNode) {
    let { recordingData } = this._getRecordingInfo();
    let sampleTimes = frameNode.sampleTimes;
    let beginAt = sampleTimes[0].start;
    let endAt = sampleTimes[sampleTimes.length - 1].end;

    // Hide the "new tab" button since a selection won't implicitly be made
    // in the newly created tab.
    this._newtabButton.hidden = true;

    yield this.addTabAndPopulate(recordingData, beginAt, endAt, { skipCallTree: true });
    this._populateCallTreeFromFrameNode(this._getPanel(), frameNode);

    // Signal that a new tab was spawned from a node in the call tree.
    window.emit(EVENTS.TAB_SPAWNED_FROM_FRAME_NODE);
  }),

  /**
   * Filters the recording data displayed in the call tree view to match
   * the current selection range in the graphs.
   *
   * @param object options
   *        Additional options supported by this operation.
   *        @see ProfileView._populatePanelWidgets
   */
  _zoomTreeFromSelection: function(options) {
    let { recordingData, displayRange } = this._getRecordingInfo();
    let categoriesGraph = this._getCategoriesGraph();
    let selectedPanel = this._getPanel();

    // If there's no selection, get the original display range and hide the
    // "new tab" button.
    if (!categoriesGraph.hasSelection()) {
      let { beginAt, endAt } = displayRange;
      this._newtabButton.hidden = true;
      this._populateCallTree(selectedPanel, recordingData.profilerData, beginAt, endAt, options);
    }
    // Otherwise, just get the selected display range and only show the
    // "new tab" button if the selection is wide enough.
    else {
      let { min: beginAt, max: endAt } = categoriesGraph.getMappedSelection();
      this._newtabButton.hidden = (endAt - beginAt) < GRAPH_ZOOM_MIN_TIMESPAN;
      this._populateCallTree(selectedPanel, recordingData.profilerData, beginAt, endAt, options);
    }
  },

  /**
   * Highlights certain areas in the categories graph to match the currently
   * selected frame node's sample times in the tree view.
   *
   * @param ThreadNode | FrameNode frameNode
   *        The root node data source for this tree.
   */
  _highlightAreaFromFrameNode: function(frameNode) {
    let categoriesGraph = this._getCategoriesGraph();
    if (categoriesGraph) {
      categoriesGraph.setMask(frameNode.sampleTimes);
    }
  },

  /**
   * Populates all the widgets in the specified tab's panel with the provided
   * data. The already existing widgets will be removed.
   *
   * @param nsIDOMNode panel
   *        The <panel> element in this <tabbox>.
   * @param object dataSource
   *        The profiler, framerate and categories data source.
   * @param number beginAt
   *        The earliest allowed time for tree nodes (in milliseconds).
   * @param number endAt
   *        The latest allowed time for tree nodes (in milliseconds).
   * @param object options
   *        Additional options supported by this operation:
   *          - skipCallTree: true if the call tree should not be populated
   *          - skipCallTreeFocus: true if the root node shouldn't be focused
   */
  _populatePanelWidgets: Task.async(function*(panel, dataSource, beginAt, endAt, options = {}) {
    let { profilerData, framerateData, categoriesData } = dataSource;

    let framerateGraph = yield this._populateFramerateGraph(panel, framerateData, beginAt);
    let categoriesGraph = yield this._populateCategoriesGraph(panel, categoriesData, beginAt);
    CanvasGraphUtils.linkAnimation(framerateGraph, categoriesGraph);
    CanvasGraphUtils.linkSelection(framerateGraph, categoriesGraph);

    if (!options.skipCallTree) {
      this._populateCallTree(panel, profilerData, beginAt, endAt, options);
    }
  }),

  /**
   * Populates the framerate graph in the specified tab's panel with the
   * provided data. The already existing graph will be removed.
   *
   * @param nsIDOMNode panel
   *        The <panel> element in this <tabbox>.
   * @param object framerateData
   *        The data source for this graph.
   * @param number beginAt
   *        The earliest time in the recording data to start at (in milliseconds).
   */
  _populateFramerateGraph: Task.async(function*(panel, framerateData, beginAt) {
    let oldGraph = this._getFramerateGraph(panel);
    if (oldGraph) {
      oldGraph.destroy();
    }

    // Don't create a graph if there's not enough data to show.
    if (!framerateData || framerateData.length < 2) {
      return null;
    }

    let graph = new LineGraphWidget($(".framerate", panel), L10N.getStr("graphs.fps"));
    graph.fixedHeight = FRAMERATE_GRAPH_HEIGHT;
    graph.minDistanceBetweenPoints = 1;
    graph.dataOffsetX = beginAt;

    yield graph.setDataWhenReady(framerateData);

    graph.on("mouseup", this._onGraphMouseUp);
    graph.on("scroll", this._onGraphScroll);

    this._framerateGraphByPanel.set(panel, graph);
    return graph;
  }),

  /**
   * Populates the categories graph in the specified tab's panel with the
   * provided data. The already existing graph will be removed.
   *
   * @param nsIDOMNode panel
   *        The <panel> element in this <tabbox>.
   * @param object categoriesData
   *        The data source for this graph.
   * @param number beginAt
   *        The earliest time in the recording data to start at (in milliseconds).
   */
  _populateCategoriesGraph: Task.async(function*(panel, categoriesData, beginAt) {
    let oldGraph = this._getCategoriesGraph(panel);
    if (oldGraph) {
      oldGraph.destroy();
    }
    // Don't create a graph if there's not enough data to show.
    if (!categoriesData || categoriesData.length < 2) {
      return null;
    }

    let graph = new BarGraphWidget($(".categories", panel));
    graph.fixedHeight = CATEGORIES_GRAPH_HEIGHT;
    graph.minBarsWidth = CATEGORIES_GRAPH_MIN_BARS_WIDTH;
    graph.format = CATEGORIES.sort((a, b) => a.ordinal > b.ordinal);
    graph.dataOffsetX = beginAt;

    yield graph.setDataWhenReady(categoriesData);

    graph.on("legend-selection", this._onGraphLegendSelection);
    graph.on("mouseup", this._onGraphMouseUp);
    graph.on("scroll", this._onGraphScroll);

    this._categoriesGraphByPanel.set(panel, graph);
    return graph;
  }),

  /**
   * Populates the call tree view in the specified tab's panel with the
   * provided data. The already existing tree will be removed.
   *
   * @param nsIDOMNode panel
   *        The <panel> element in this <tabbox>.
   * @param object profilerData
   *        The data source for this tree.
   * @param number beginAt
   *        The earliest time in the data source to start at (in milliseconds).
   * @param number endAt
   *        The latest time in the data source to end at (in milliseconds).
   * @param object options
   *        Additional options supported by this operation.
   *        @see ProfileView._populatePanelWidgets
   */
  _populateCallTree: function(panel, profilerData, beginAt, endAt, options) {
    let threadSamples = profilerData.profile.threads[0].samples;
    let contentOnly = !Prefs.showPlatformData;
    let threadNode = new ThreadNode(threadSamples, contentOnly, beginAt, endAt);
    this._populateCallTreeFromFrameNode(panel, threadNode, options);
  },

  /**
   * Populates the call tree view in the specified tab's panel with the
   * provided frame node. The already existing tree will be removed.
   *
   * @param nsIDOMNode panel
   *        The <panel> element in this <tabbox>.
   * @param ThreadNode | FrameNode frameNode
   *        The root node data source for this tree.
   * @param object options
   *        Additional options supported by this operation.
   *        @see ProfileView._populatePanelWidgets
   */
  _populateCallTreeFromFrameNode: function(panel, frameNode, options = {}) {
    let oldRoot = this._getCallTreeRoot(panel);
    if (oldRoot) {
      oldRoot.remove();
    }

    let callTreeRoot = new CallView({ frame: frameNode });
    callTreeRoot.on("focus", this._onCallViewFocus);
    callTreeRoot.on("link", this._onCallViewLink);
    callTreeRoot.on("zoom", this._onCallViewZoom);
    callTreeRoot.attachTo($(".call-tree-cells-container", panel));

    if (!options.skipCallTreeFocus) {
      callTreeRoot.focus();
    }

    let contentOnly = !Prefs.showPlatformData;
    callTreeRoot.toggleCategories(!contentOnly);

    this._callTreeRootByPanel.set(panel, callTreeRoot);
  },

  /**
   * Shortcuts for accessing the recording info or widgets for a <panel>.
   * @param nsIDOMNode panel [optional]
   * @return object
   */
  _getRecordingInfo: function(panel = this._getPanel()) {
    return this._recordingInfoByPanel.get(panel);
  },
  _getFramerateGraph: function(panel = this._getPanel()) {
    return this._framerateGraphByPanel.get(panel);
  },
  _getCategoriesGraph: function(panel = this._getPanel()) {
    return this._categoriesGraphByPanel.get(panel);
  },
  _getCallTreeRoot: function(panel = this._getPanel()) {
    return this._callTreeRootByPanel.get(panel);
  },
  _getTab: function(tabIndex = this._getSelectedIndex()) {
    return this._tabs.childNodes[tabIndex];
  },
  _getPanel: function(tabIndex = this._getSelectedIndex()) {
    return this._panels.childNodes[tabIndex];
  },
  _getSelectedIndex: function() {
    return $("#profile-content").selectedIndex;
  },

  /**
   * Listener handling the tab "select" event in this container.
   */
  _onTabSelect: function() {
    let categoriesGraph = this._getCategoriesGraph();
    if (categoriesGraph) {
      this._newtabButton.hidden = !categoriesGraph.hasSelection();
    } else {
      this._newtabButton.hidden = true;
    }

    this.removeTabsAfter(this._getSelectedIndex());
  },

  /**
   * Listener handling the new tab "click" event in this container.
   */
  _onNewTabClick: function() {
    this._spawnTabFromSelection();
  },

  /**
   * Listener handling the "legend-selection" event for the graphs in this container.
   */
  _onGraphLegendSelection: function() {
    this._zoomTreeFromSelection({ skipCallTreeFocus: true });
  },

  /**
   * Listener handling the "mouseup" event for the graphs in this container.
   */
  _onGraphMouseUp: function() {
    this._zoomTreeFromSelection();
  },

  /**
   * Listener handling the "scroll" event for the graphs in this container.
   */
  _onGraphScroll: function() {
    setNamedTimeout("graph-scroll", GRAPH_SCROLL_EVENTS_DRAIN, () => {
      this._zoomTreeFromSelection();
    });
  },

  /**
   * Listener handling the "focus" event for the call tree in this container.
   */
  _onCallViewFocus: function(event, treeItem) {
    setNamedTimeout("graph-focus", CALL_VIEW_FOCUS_EVENTS_DRAIN, () => {
      this._highlightAreaFromFrameNode(treeItem.frame);
    });
  },

  /**
   * Listener handling the "link" event for the call tree in this container.
   */
  _onCallViewLink: function(event, treeItem) {
    let { url, line } = treeItem.frame.getInfo();
    viewSourceInDebugger(url, line);
  },

  /**
   * Listener handling the "zoom" event for the call tree in this container.
   */
  _onCallViewZoom: function(event, treeItem) {
    this._spawnTabFromFrameNode(treeItem.frame);
  }
};

/**
 * Utility functions handling recording data.
 */
let RecordingUtils = {
  _frameratePlotsCache: new WeakMap(),

  /**
   * Creates an appropriate data source to be displayed in a categories graph
   * from on the provided profiler data.
   *
   * @param object profilerData
   *        The profiler data received from the front.
   * @param number beginAt
   *        The earliest time in the profiler data to start at (in milliseconds).
   * @param number endAt
   *        The latest time in the profiler data to end at (in milliseconds).
   * @return array
   *         A data source useful for a BarGraphWidget.
   */
  plotCategoriesFor: function(profilerData, beginAt, endAt) {
    let categoriesData = [];
    let profile = profilerData.profile;
    let samples = profile.threads[0].samples;

    // Accumulate the category of each frame for every sample.
    for (let { frames, time } of samples) {
      if (!time || time < beginAt || time > endAt) continue;
      let blocks = [];

      for (let { category: bitmask } of frames) {
        if (!bitmask) continue;
        let category = CATEGORY_MAPPINGS[bitmask];

        // Guard against categories that aren't found in the frontend mappings.
        // This usually means that a new category was added in the platform,
        // but browser/devtools/profiler/utils/global.js wasn't updated yet.
        if (!category) {
          category = CATEGORY_MAPPINGS[CATEGORY_OTHER];
        }

        if (!blocks[category.ordinal]) {
          blocks[category.ordinal] = 1;
        } else {
          blocks[category.ordinal]++;
        }
      }

      // If no categories were found in the frames, default to using a
      // single block using the stack depth as height.
      if (blocks.length == 0) {
        blocks[CATEGORY_MAPPINGS[CATEGORY_OTHER].ordinal] = frames.length;
      }

      categoriesData.push({
        delta: time,
        values: blocks
      });
    }

    return categoriesData;
  },

  /**
   * Creates an appropriate data source to be displayed in a framerate graph
   * from on the provided refresh driver ticks data.
   *
   * @param object ticksData
   *        The refresh driver ticks received from the front.
   * @param number beginAt
   *        The earliest time in the ticks data to start at (in milliseconds).
   * @param number endAt
   *        The latest time in the ticks data to end at (in milliseconds).
   * @return array
   *         A data source useful for a LineGraphWidget.
   */
  plotFramerateFor: function(ticksData, beginAt, endAt) {
    // Older Gecko versions don't have a framerate actor implementation,
    // in which case the returned ticks data is null.
    if (ticksData == null) {
      return [];
    }

    let framerateData = this._frameratePlotsCache.get(ticksData);
    if (framerateData == null) {
      framerateData = FramerateFront.plotFPS(ticksData, FRAMERATE_CALC_INTERVAL);
      this._frameratePlotsCache.set(ticksData, framerateData);
    }

    // Quickly find the earliest and oldest valid index in the plotted
    // framerate data based on the specified beginAt and endAt time. Sure,
    // using [].findIndex would be more elegant, but also slower.
    let earliestValidIndex = findFirstIndex(framerateData, e => e.delta >= beginAt);
    let oldestValidIndex = findLastIndex(framerateData, e => e.delta <= endAt);
    let totalValues = framerateData.length;

    // If all the plotted framerate data fits inside the specified time range,
    // simply return it.
    if (earliestValidIndex == 0 && oldestValidIndex == totalValues - 1) {
      return framerateData;
    }

    // Otherwise, a slice will need to be made. Be very careful here, the
    // beginAt and endAt timestamps can refer to a point in *between* two
    // entries in the framerate data, so we'll need to insert new values where
    // the cuts are made.
    let slicedData = framerateData.slice(earliestValidIndex, oldestValidIndex + 1);
    if (earliestValidIndex > 0) {
      slicedData.unshift({
        delta: beginAt,
        value: framerateData[earliestValidIndex - 1].value
      });
    }
    if (oldestValidIndex < totalValues - 1) {
      slicedData.push({
        delta: endAt,
        value: framerateData[oldestValidIndex + 1].value
      });
    }

    return slicedData;
  },

  /**
   * Makes sure the data sources for the categories and framerate graphs
   * have the same beginning and ending, time-wise.
   *
   * @param array categoriesData
   *        Data source generated by `RecordingUtils.plotCategoriesFor`.
   * @param array framerateData
   *        Data source generated by `RecordingUtils.plotFramerateFor`.
   */
  syncCategoriesWithFramerate: function(categoriesData, framerateData) {
    if (categoriesData.length < 2 || framerateData.length < 2) {
      return;
    }
    let categoryBegin = categoriesData[0];
    let categoryEnd = categoriesData[categoriesData.length - 1];
    let framerateBegin = framerateData[0];
    let framerateEnd = framerateData[framerateData.length - 1];

    if (categoryBegin.delta > framerateBegin.delta) {
      categoriesData.unshift({
        delta: framerateBegin.delta,
        values: categoryBegin.values
      });
    } else {
      framerateData.unshift({
        delta: categoryBegin.delta,
        value: framerateBegin.value
      });
    }
    if (categoryEnd.delta < framerateEnd.delta) {
      categoriesData.push({
        delta: framerateEnd.delta,
        values: categoryEnd.values
      });
    } else {
      framerateData.push({
        delta: categoryEnd.delta,
        value: framerateEnd.value
      });
    }
  }
};

/**
 * Finds the index of the first element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findFirstIndex(array, predicate) {
  for (let i = 0, len = array.length; i < len; i++) {
    if (predicate(array[i])) return i;
  }
}

/**
 * Finds the last of the first element in an array that validates a predicate.
 * @param array
 * @param function predicate
 * @return number
 */
function findLastIndex(array, predicate) {
  for (let i = array.length - 1; i >= 0; i--) {
    if (predicate(array[i])) return i;
  }
}

/**
 * Opens/selects the debugger in this toolbox and jumps to the specified
 * file name and line number.
 * @param string url
 * @param number line
 */
function viewSourceInDebugger(url, line) {
  let showSource = ({ DebuggerView }) => {
    if (DebuggerView.Sources.containsValue(url)) {
      DebuggerView.setEditorLocation(url, line, { noDebug: true }).then(() => {
        window.emit(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
      }, () => {
        window.emit(EVENTS.SOURCE_NOT_FOUND_IN_JS_DEBUGGER);
      });
    }
  };

  // If the Debugger was already open, switch to it and try to show the
  // source immediately. Otherwise, initialize it and wait for the sources
  // to be added first.
  let debuggerAlreadyOpen = gToolbox.getPanel("jsdebugger");
  gToolbox.selectTool("jsdebugger").then(({ panelWin: dbg }) => {
    if (debuggerAlreadyOpen) {
      showSource(dbg);
    } else {
      dbg.once(dbg.EVENTS.SOURCES_ADDED, () => showSource(dbg));
    }
  });
}
