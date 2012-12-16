/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EIDETICKER_BASE_URL = "http://eideticker.wrla.ch/";

var gDebugLog = false;
var gDebugTrace = false;
var gLocation = window.location + "";
if (gLocation.indexOf("file:") == 0) {
  gDebugLog = true;
  gDebugTrace = true;
  PROFILERLOG("Turning on logging+tracing since cleopatra is served from the file protocol");
}
// Use for verbose tracing, otherwise use log
function PROFILERTRACE(msg) {
  if (gDebugTrace)
    PROFILERLOG(msg);
}
function PROFILERLOG(msg) {
  if (gDebugLog) {
    msg = "Cleo: " + msg;
    console.log(msg);
    if (window.dump)
      window.dump(msg + "\n");
  }
}
function PROFILERERROR(msg) {
  msg = "Cleo: " + msg;
  console.log(msg);
  if (window.dump)
    window.dump(msg + "\n");
}
function enableProfilerTracing() {
  gDebugLog = true;
  gDebugTrace = true;
  Parser.updateLogSetting();
}
function enableProfilerLogging() {
  gDebugLog = true;
  Parser.updateLogSetting();
}

function removeAllChildren(element) {
  while (element.firstChild) {
    element.removeChild(element.firstChild);
  }
}

function FileList() {
  this._container = document.createElement("ul");
  this._container.id = "fileList";
  this._selectedFileItem = null;
  this._fileItemList = [];
}

FileList.prototype = {
  getContainer: function FileList_getContainer() {
    return this._container;
  },

  loadProfileListFromLocalStorage: function FileList_loadProfileListFromLocalStorage() {
    var self = this;
    gLocalStorage.getProfileList(function(profileList) {
      for (var i = 0; i < profileList.length; i++) {
        (function closure() {
          // This only carries info about the profile and the access key to retrieve it.
          var profileInfo = profileList[i];
          //PROFILERTRACE("Profile list from local storage: " + JSON.stringify(profileInfo));
          var fileEntry = self.addFile(profileInfo.profileKey, "local storage", function fileEntryClick() {
            loadLocalStorageProfile(profileInfo.profileKey);
          });
        })();
      }
    });
  },

  addFile: function FileList_addFile(fileName, description, onselect) {
    var li = document.createElement("li");

    li.fileName = fileName || "New Profile";
    li.description = description || "(empty)";

    li.className = "fileListItem";
    if (!this._selectedFileItem) {
      li.classList.add("selected");
      this._selectedFileItem = li;
    }

    li.onselect = onselect;
    var self = this;
    li.onclick = function() {
      self.setSelection(li);
    }

    var fileListItemTitleSpan = document.createElement("span");
    fileListItemTitleSpan.className = "fileListItemTitle";
    fileListItemTitleSpan.textContent = li.fileName;
    li.appendChild(fileListItemTitleSpan);

    var fileListItemDescriptionSpan = document.createElement("span");
    fileListItemDescriptionSpan.className = "fileListItemDescription";
    fileListItemDescriptionSpan.textContent = li.description;
    li.appendChild(fileListItemDescriptionSpan);

    this._container.appendChild(li);

    this._fileItemList.push(li);

    return li;
  },

  setSelection: function FileList_setSelection(fileEntry) {
    if (this._selectedFileItem) {
      this._selectedFileItem.classList.remove("selected");
    }
    this._selectedFileItem = fileEntry;
    fileEntry.classList.add("selected");
    if (this._selectedFileItem.onselect)
      this._selectedFileItem.onselect();
  },

  profileParsingFinished: function FileList_profileParsingFinished() {
    this._container.querySelector(".fileListItemTitle").textContent = "Current Profile";
    this._container.querySelector(".fileListItemDescription").textContent = gNumSamples + " Samples";
  }
}

function treeObjSort(a, b) {
  return b.counter - a.counter;
}

function ProfileTreeManager() {
  this.treeView = new TreeView();
  this.treeView.setColumns([
    { name: "sampleCount", title: "Running time" },
    { name: "selfSampleCount", title: "Self" },
    { name: "symbolName", title: "Symbol Name"},
  ]);
  var self = this;
  this.treeView.addEventListener("select", function (frameData) {
    self.highlightFrame(frameData);
  });
  this.treeView.addEventListener("contextMenuClick", function (e) {
    self._onContextMenuClick(e);
  });
  this.treeView.addEventListener("focusCallstackButtonClicked", function (frameData) {
    var focusedCallstack = self._getCallstackUpTo(frameData);
    focusOnCallstack(focusedCallstack, frameData.name);
  });
  this._container = document.createElement("div");
  this._container.className = "tree";
  this._container.appendChild(this.treeView.getContainer());

  // If this is set when the tree changes the snapshot is immediately restored.
  this._savedSnapshot = null;
}
ProfileTreeManager.prototype = {
  getContainer: function ProfileTreeManager_getContainer() {
    return this._container;
  },
  highlightFrame: function Treedisplay_highlightFrame(frameData) {
    setHighlightedCallstack(this._getCallstackUpTo(frameData), this._getHeaviestCallstack(frameData));
  },
  dataIsOutdated: function ProfileTreeManager_dataIsOutdated() {
    this.treeView.dataIsOutdated();
  },
  saveSelectionSnapshot: function ProfileTreeManager_getSelectionSnapshot(isJavascriptOnly) {
    this._savedSnapshot = this.treeView.getSelectionSnapshot(isJavascriptOnly);
  },
  saveReverseSelectionSnapshot: function ProfileTreeManager_getReverseSelectionSnapshot(isJavascriptOnly) {
    this._savedSnapshot = this.treeView.getReverseSelectionSnapshot(isJavascriptOnly);
  },
  _restoreSelectionSnapshot: function ProfileTreeManager__restoreSelectionSnapshot(snapshot, allowNonContigous) {
    return this.treeView.restoreSelectionSnapshot(snapshot, allowNonContigous);
  },
  setSelection: function ProfileTreeManager_setSelection(frames) {
    return this.treeView.setSelection(frames);
  },
  _getCallstackUpTo: function ProfileTreeManager__getCallstackUpTo(frame) {
    var callstack = [];
    var curr = frame;
    while (curr != null) {
      if (curr.name != null) {
        var subCallstack = curr.fullFrameNamesAsInSample.clone();
        subCallstack.reverse();
        callstack = callstack.concat(subCallstack);
      }
      curr = curr.parent;
    }
    callstack.reverse();
    if (gInvertCallstack)
      callstack.shift(); // remove (total)
    return callstack;
  },
  _getHeaviestCallstack: function ProfileTreeManager__getHeaviestCallstack(frame) {
    // FIXME: This gets the first leaf which is not the heaviest leaf.
    while(frame.children && frame.children.length > 0) {
      var nextFrame = frame.children[0].getData();
      if (!nextFrame)
        break;
      frame = nextFrame;
    }
    return this._getCallstackUpTo(frame);
  },
  _onContextMenuClick: function ProfileTreeManager__onContextMenuClick(e) {
    var node = e.node;
    var menuItem = e.menuItem;

    if (menuItem == "View Source") {
      // Remove anything after ( since MXR doesn't handle search with the arguments.
      var symbol = node.name.split("(")[0];
      window.open("http://mxr.mozilla.org/mozilla-central/search?string=" + symbol, "View Source");
    } else if (menuItem == "View JS Source") {
      viewJSSource(node);
    } else if (menuItem == "Plugin View: Pie") {
      focusOnPluginView("protovis", {type:"pie"});
    } else if (menuItem == "Plugin View: Tree") {
      focusOnPluginView("protovis", {type:"tree"});
    } else if (menuItem == "Google Search") {
      var symbol = node.name;
      window.open("https://www.google.ca/search?q=" + symbol, "View Source");
    } else if (menuItem == "Focus Frame") {
      var symbol = node.fullFrameNamesAsInSample[0]; // TODO: we only function one symbol when callpath merging is on, fix that
      focusOnSymbol(symbol, node.name);
    } else if (menuItem == "Focus Callstack") {
      var focusedCallstack = this._getCallstackUpTo(node);
      focusOnCallstack(focusedCallstack, node.name);
    }
  },
  setAllowNonContigous: function ProfileTreeManager_setAllowNonContigous() {
    this._allowNonContigous = true;
  },
  display: function ProfileTreeManager_display(tree, symbols, functions, useFunctions, filterByName) {
    this.treeView.display(this.convertToJSTreeData(tree, symbols, functions, useFunctions), filterByName);
    if (this._savedSnapshot) {
      this._restoreSelectionSnapshot(this._savedSnapshot, this._allowNonContigous);
      this._savedSnapshot = null;
      this._allowNonContigous = false;
    }
  },
  convertToJSTreeData: function ProfileTreeManager__convertToJSTreeData(rootNode, symbols, functions, useFunctions) {
    var totalSamples = rootNode.counter;
    function createTreeViewNode(node, parent) {
      var curObj = {};
      curObj.parent = parent;
      curObj.counter = node.counter;
      var selfCounter = node.counter;
      for (var i = 0; i < node.children.length; ++i) {
        selfCounter -= node.children[i].counter;
      }
      curObj.selfCounter = selfCounter;
      curObj.ratio = node.counter / totalSamples;
      curObj.fullFrameNamesAsInSample = node.mergedNames ? node.mergedNames : [node.name];
      if (useFunctions ? !(node.name in functions) : !(node.name in symbols)) {
        curObj.name = node.name;
        curObj.library = "";
      } else {
        var functionObj = useFunctions ? functions[node.name] : functions[symbols[node.name].functionIndex];
        var info = {
          functionName: functionObj.functionName,
          libraryName: functionObj.libraryName,
          lineInformation: useFunctions ? "" : symbols[node.name].lineInformation
        };
        curObj.name = (info.functionName + " " + info.lineInformation).trim();
        curObj.library = info.libraryName;
        curObj.isJSFrame = functionObj.isJSFrame;
        if (functionObj.scriptLocation) {
          curObj.scriptLocation = functionObj.scriptLocation;
        }
      }
      if (node.children.length) {
        curObj.children = getChildrenObjects(node.children, curObj);
      }
      return curObj;
    }
    function getChildrenObjects(children, parent) {
      var sortedChildren = children.slice(0).sort(treeObjSort);
      return sortedChildren.map(function (child) {
        var createdNode = null;
        return {
          getData: function () {
            if (!createdNode) {
              createdNode = createTreeViewNode(child, parent);
            }
            return createdNode;
          }
        };
      });
    }
    return getChildrenObjects([rootNode], null);
  },
};

function SampleBar() {
  this._container = document.createElement("div");
  this._container.id = "sampleBar";
  this._container.className = "sideBar";

  this._header = document.createElement("h2");
  this._header.innerHTML = "Selection - Most time spent in:";
  this._header.alt = "This shows the heaviest leaf of the selected sample. Use this to get a quick glimpse of where the selection is spending most of its time.";
  this._container.appendChild(this._header);

  this._text = document.createElement("span");
  this._text.style.whiteSpace = "pre";
  this._text.innerHTML = "Sample text";
  this._container.appendChild(this._text);
}

SampleBar.prototype = {
  getContainer: function SampleBar_getContainer() {
    return this._container;
  },
  setSample: function SampleBar_setSample(sample) {
    var str = "";
    var list = [];

    this._text.innerHTML = "";

    for (var i = 0; i < sample.length; i++) {
      var functionObj = gMergeFunctions ? gFunctions[sample[i]] : gFunctions[symbols[sample[i]].functionIndex];
      if (!functionObj)
        continue;
      var functionLink = document.createElement("a");
      functionLink.textContent = "- " + functionObj.functionName;
      functionLink.href = "#";
      this._text.appendChild(functionLink);
      this._text.appendChild(document.createElement("br"));
      list.push(functionObj.functionName);
      functionLink.selectIndex = i;
      functionLink.onclick = function() {
        var selectedFrames = [];
        if (gInvertCallstack) {
          for (var i = 0; i <= this.selectIndex; i++) {
            var functionObj = gMergeFunctions ? gFunctions[sample[i]] : gFunctions[symbols[sample[i]].functionIndex];
            selectedFrames.push(functionObj.functionName);
          }
        } else {
          for (var i = sample.length - 1; i >= this.selectIndex; i--) {
            var functionObj = gMergeFunctions ? gFunctions[sample[i]] : gFunctions[symbols[sample[i]].functionIndex];
            selectedFrames.push(functionObj.functionName);
          }
        }
        gTreeManager.setSelection(selectedFrames);
        return false;
      }
    }
    return list;
  },
}


function PluginView() {
  this._container = document.createElement("div");
  this._container.className = "pluginview";
  this._container.style.visibility = 'hidden';
  this._iframe = document.createElement("iframe");
  this._iframe.className = "pluginviewIFrame";
  this._container.appendChild(this._iframe);
  this._container.style.top = "";
}
PluginView.prototype = {
  getContainer: function PluginView_getContainer() {
    return this._container;
  },
  hide: function() {
    // get rid of the scrollbars
    this._container.style.top = "";
    this._container.style.visibility = 'hidden';
  },
  show: function() {
    // This creates extra scrollbar so only do it when needed
    this._container.style.top = "0px";
    this._container.style.visibility = '';
  },
  display: function(pluginName, param, data) {
    this._iframe.src = "js/plugins/" + pluginName + "/index.html";
    var self = this;
    this._iframe.onload = function() {
      console.log("Pluginview '" + pluginName + " iframe onload");
      self._iframe.contentWindow.initCleopatraPlugin(data, param, gSymbols);
    }
    this.show();
    //console.log(gSymbols);
  },
}

function HistogramView() {
  this._container = document.createElement("div");
  this._container.className = "histogram";

  this._canvas = this._createCanvas();
  this._container.appendChild(this._canvas);

  this._rangeSelector = new RangeSelector(this._canvas, this);
  this._rangeSelector.enableRangeSelectionOnHistogram();
  this._container.appendChild(this._rangeSelector.getContainer());

  this._busyCover = document.createElement("div");
  this._busyCover.className = "busyCover";
  this._container.appendChild(this._busyCover);

  this._histogramData = [];
}
HistogramView.prototype = {
  dataIsOutdated: function HistogramView_dataIsOutdated() {
    this._busyCover.classList.add("busy");
  },
  _createCanvas: function HistogramView__createCanvas() {
    var canvas = document.createElement("canvas");
    canvas.height = 60;
    canvas.style.width = "100%";
    canvas.style.height = "100%";
    return canvas;
  },
  getContainer: function HistogramView_getContainer() {
    return this._container;
  },
  showVideoFramePosition: function HistogramView_showVideoFramePosition(frame) {
    if (!this._frameStart || !this._frameStart[frame])
      return;
    var frameStart = this._frameStart[frame];
    // Now we look for the frame end. Because we can swap frame we don't present we have to look ahead
    // in the stream if frame+1 doesn't exist.
    var frameEnd = this._frameStart[frame+1];
    for (var i = 0; i < 10 && !frameEnd; i++) {
      frameEnd = this._frameStart[frame+1+i];
    }
    this._rangeSelector.showVideoRange(frameStart, frameEnd);
  },
  showVideoPosition: function HistogramView_showVideoPosition(position) {
    // position in 0..1
    this._rangeSelector.showVideoPosition(position);
  },
  _gatherMarkersList: function HistogramView__gatherMarkersList(histogramData) {
    var markers = [];
    for (var i = 0; i < histogramData.length; ++i) {
      var step = histogramData[i];
      if ("marker" in step) {
        markers.push({
          index: i,
          name: step.marker
        });
      }
    }
    return markers;
  },
  _calculateWidthMultiplier: function () {
    var minWidth = 2000;
    return Math.ceil(minWidth / this._widthSum);
  },
  histogramClick: function HistogramView_histogramClick(index) {
    var sample = this._histogramData[index];
    var frames = sample.frames;
    if (gSampleBar) {
      var list = gSampleBar.setSample(frames[0]);
      gTreeManager.setSelection(list);
      setHighlightedCallstack(frames[0], frames[0]);
    }
  },
  display: function HistogramView_display(histogramData, frameStart, widthSum, highlightedCallstack) {
    this._histogramData = histogramData;
    PROFILERTRACE("FRAME START: " + frameStart + "\n");
    this._frameStart = frameStart;
    this._widthSum = widthSum;
    this._widthMultiplier = this._calculateWidthMultiplier();
    this._canvas.width = this._widthMultiplier * this._widthSum;
    this._render(highlightedCallstack);
    this._busyCover.classList.remove("busy");
  },
  _render: function HistogramView__render(highlightedCallstack) {
    var ctx = this._canvas.getContext("2d");
    var height = this._canvas.height;
    ctx.setTransform(this._widthMultiplier, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, this._widthSum, height);

    var self = this;
    for (var i = 0; i < this._histogramData.length; i++) {
      var step = this._histogramData[i];
      var isSelected = self._isStepSelected(step, highlightedCallstack);
      var isInRangeSelector = self._isInRangeSelector(i);
      if (isSelected) {
        ctx.fillStyle = "green";
      } else if (isInRangeSelector) {
        ctx.fillStyle = "blue";
      } else {
        ctx.fillStyle = step.color;
      }
      var roundedHeight = Math.round(step.value * height);
      ctx.fillRect(step.x, height - roundedHeight, step.width, roundedHeight);
    }

    this._finishedRendering = true;
  },
  highlightedCallstackChanged: function HistogramView_highlightedCallstackChanged(highlightedCallstack) {
    this._render(highlightedCallstack);
  },
  _isInRangeSelector: function HistogramView_isInRangeSelector(index) {
    return false;
  },
  _isStepSelected: function HistogramView__isStepSelected(step, highlightedCallstack) {
    if ("marker" in step)
      return false;
    return step.frames.some(function isCallstackSelected(frames) {
      if (frames.length < highlightedCallstack.length ||
          highlightedCallstack.length <= (gInvertCallstack ? 0 : 1))
        return false;

      var compareFrames = frames;
      if (gInvertCallstack) {
        for (var j = 0; j < highlightedCallstack.length; j++) {
          var compareFrameIndex = compareFrames.length - 1 - j;
          if (highlightedCallstack[j] != compareFrames[compareFrameIndex]) {
            return false;
          }
        }
      } else {
        for (var j = 0; j < highlightedCallstack.length; j++) {
          var compareFrameIndex = j;
          if (highlightedCallstack[j] != compareFrames[compareFrameIndex]) {
            return false;
          }
        }
      }
      return true;
    });
  },
  getHistogramData: function HistogramView__getHistogramData() {
    return this._histogramData;
  },
  _getStepColor: function HistogramView__getStepColor(step) {
      if ("responsiveness" in step.extraInfo) {
        var res = step.extraInfo.responsiveness;
        var redComponent = Math.round(255 * Math.min(1, res / kDelayUntilWorstResponsiveness));
        return "rgb(" + redComponent + ",0,0)";
      }

      return "rgb(0,0,0)";
  },
};

function RangeSelector(graph, histogram) {
  this._histogram = histogram;
  this.container = document.createElement("div");
  this.container.className = "rangeSelectorContainer";
  this._graph = graph;
  this._selectedRange = { startX: 0, endX: 0 };
  this._selectedSampleRange = { start: 0, end: 0 };

  this._highlighter = document.createElement("div");
  this._highlighter.className = "histogramHilite collapsed";
  this.container.appendChild(this._highlighter);

  this._mouseMarker = document.createElement("div");
  this._mouseMarker.className = "histogramMouseMarker";
  this.container.appendChild(this._mouseMarker);
}
RangeSelector.prototype = {
  getContainer: function RangeSelector_getContainer() {
    return this.container;
  },
  // echo the location off the mouse on the histogram
  drawMouseMarker: function RangeSelector_drawMouseMarker(x) {
    console.log("Draw");
    var mouseMarker = this._mouseMarker;
    mouseMarker.style.left = x + "px";
  },
  showVideoPosition: function RangeSelector_showVideoPosition(position) {
    this.drawMouseMarker(position * (this._graph.parentNode.clientWidth-1));
    PROFILERLOG("Show video position: " + position);
  },
  drawHiliteRectangle: function RangeSelector_drawHiliteRectangle(x, y, width, height) {
    var hilite = this._highlighter;
    hilite.style.left = x + "px";
    hilite.style.top = "0";
    hilite.style.width = width + "px";
    hilite.style.height = height + "px";
  },
  clearCurrentRangeSelection: function RangeSelector_clearCurrentRangeSelection() {
    try {
      this.changeEventSuppressed = true;
      var children = this.selector.childNodes;
      for (var i = 0; i < children.length; ++i) {
        children[i].selected = false;
      }
    } finally {
      this.changeEventSuppressed = false;
    }
  },
  showVideoRange: function RangeSelector_showVideoRange(startIndex, endIndex) {
    if (!endIndex || endIndex < 0)
      endIndex = gCurrentlyShownSampleData.length;

    var len = this._graph.parentNode.getBoundingClientRect().right - this._graph.parentNode.getBoundingClientRect().left;
    this._selectedRange.startX = startIndex * len / this._histogram._histogramData.length;
    this._selectedRange.endX = endIndex * len / this._histogram._histogramData.length;
    var width = this._selectedRange.endX - this._selectedRange.startX;
    var height = this._graph.parentNode.clientHeight;
    this._highlighter.classList.remove("collapsed");
    this.drawHiliteRectangle(this._selectedRange.startX, 0, width, height);
    //this._finishSelection(startIndex, endIndex);
  },
  enableRangeSelectionOnHistogram: function RangeSelector_enableRangeSelectionOnHistogram() {
    var graph = this._graph;
    var isDrawingRectangle = false;
    var origX, origY;
    var self = this;
    function histogramClick(clickX, clickY) {
      clickX = Math.min(clickX, graph.parentNode.getBoundingClientRect().right);
      clickX = clickX - graph.parentNode.getBoundingClientRect().left;
      var index = self._histogramIndexFromPoint(clickX);
      self._histogram.histogramClick(index);
    }
    function updateHiliteRectangle(newX, newY) {
      newX = Math.min(newX, graph.parentNode.getBoundingClientRect().right);
      var startX = Math.min(newX, origX) - graph.parentNode.getBoundingClientRect().left;
      var startY = 0;
      var width = Math.abs(newX - origX);
      var height = graph.parentNode.clientHeight;
      if (startX < 0) {
        width += startX;
        startX = 0;
      }
      self._selectedRange.startX = startX;
      self._selectedRange.endX = startX + width;
      self.drawHiliteRectangle(startX, startY, width, height);
    }
    function updateMouseMarker(newX) {
      self.drawMouseMarker(newX - graph.parentNode.getBoundingClientRect().left);
    }
    graph.addEventListener("mousedown", function(e) {
      if (e.button != 0)
        return;
      graph.style.cursor = "col-resize";
      isDrawingRectangle = true;
      self.beginHistogramSelection();
      origX = e.pageX;
      origY = e.pageY;
      if (this.setCapture)
        this.setCapture();
      // Reset the highlight rectangle
      updateHiliteRectangle(e.pageX, e.pageY);
      e.preventDefault();
      this._movedDuringClick = false;
    }, false);
    graph.addEventListener("mouseup", function(e) {
      graph.style.cursor = "default";
      if (!this._movedDuringClick) {
        isDrawingRectangle = false;
        // Handle as a click on the histogram. Select the sample:
        histogramClick(e.pageX, e.pageY);
      } else if (isDrawingRectangle) {
        isDrawingRectangle = false;
        updateHiliteRectangle(e.pageX, e.pageY);
        self.finishHistogramSelection(e.pageX != origX);
        if (e.pageX == origX) {
          // Simple click in the histogram
          var index = self._sampleIndexFromPoint(e.pageX - graph.parentNode.getBoundingClientRect().left);
          // TODO Select this sample in the tree view
          var sample = gCurrentlyShownSampleData[index];
          console.log("Should select: " + sample);
        }
      }
    }, false);
    graph.addEventListener("mousemove", function(e) {
      this._movedDuringClick = true;
      if (isDrawingRectangle) {
        console.log(e.pageX);
        updateMouseMarker(-1); // Clear
        updateHiliteRectangle(e.pageX, e.pageY);
      } else {
        updateMouseMarker(e.pageX);
      }
    }, false);
    graph.addEventListener("mouseout", function(e) {
      updateMouseMarker(-1); // Clear
    }, false);
  },
  beginHistogramSelection: function RangeSelector_beginHistgramSelection() {
    var hilite = this._highlighter;
    hilite.classList.remove("finished");
    hilite.classList.add("selecting");
    hilite.classList.remove("collapsed");
    if (this._transientRestrictionEnteringAffordance) {
      this._transientRestrictionEnteringAffordance.discard();
    }
  },
  _finishSelection: function RangeSelector__finishSelection(start, end) {
    var newFilterChain = gSampleFilters.concat({ type: "RangeSampleFilter", start: start, end: end });
    var self = this;
    self._transientRestrictionEnteringAffordance = gBreadcrumbTrail.add({
      title: "Sample Range [" + start + ", " + (end + 1) + "]",
      enterCallback: function () {
        gSampleFilters = newFilterChain;
        self.collapseHistogramSelection();
        filtersChanged();
      }
    });
  },
  finishHistogramSelection: function RangeSelector_finishHistgramSelection(isSomethingSelected) {
    var self = this;
    var hilite = this._highlighter;
    hilite.classList.remove("selecting");
    if (isSomethingSelected) {
      hilite.classList.add("finished");
      var start = this._sampleIndexFromPoint(this._selectedRange.startX);
      var end = this._sampleIndexFromPoint(this._selectedRange.endX);
      self._finishSelection(start, end);
    } else {
      hilite.classList.add("collapsed");
    }
  },
  collapseHistogramSelection: function RangeSelector_collapseHistogramSelection() {
    var hilite = this._highlighter;
    hilite.classList.add("collapsed");
  },
  _sampleIndexFromPoint: function RangeSelector__sampleIndexFromPoint(x) {
    // XXX this is completely wrong, fix please
    var totalSamples = parseFloat(gCurrentlyShownSampleData.length);
    var width = parseFloat(this._graph.parentNode.clientWidth);
    var factor = totalSamples / width;
    return parseInt(parseFloat(x) * factor);
  },
  _histogramIndexFromPoint: function RangeSelector__histogramIndexFromPoint(x) {
    // XXX this is completely wrong, fix please
    var totalSamples = parseFloat(this._histogram._histogramData.length);
    var width = parseFloat(this._graph.parentNode.clientWidth);
    var factor = totalSamples / width;
    return parseInt(parseFloat(x) * factor);
  },
};

function videoPaneTimeChange(video) {
  if (!gMeta || !gMeta.frameStart)
    return;

  var frame = gVideoPane.getCurrentFrameNumber();
  //var frameStart = gMeta.frameStart[frame];
  //var frameEnd = gMeta.frameStart[frame+1]; // If we don't have a frameEnd assume the end of the profile

  gHistogramView.showVideoFramePosition(frame);
}


window.onpopstate = function(ev) {
  if (!gBreadcrumbTrail)
    return;
  console.log("pop: " + JSON.stringify(ev.state));
  gBreadcrumbTrail.pop();
  if (ev.state) {
    console.log("state");
    if (ev.state.action === "popbreadcrumb") {
      console.log("bread");
      //gBreadcrumbTrail.pop();
    }
  }
}

function BreadcrumbTrail() {
  this._breadcrumbs = [];
  this._selectedBreadcrumbIndex = -1;

  this._containerElement = document.createElement("div");
  this._containerElement.className = "breadcrumbTrail";
  var self = this;
  this._containerElement.addEventListener("click", function (e) {
    if (!e.target.classList.contains("breadcrumbTrailItem"))
      return;
    self._enter(e.target.breadcrumbIndex);
  });
}
BreadcrumbTrail.prototype = {
  getContainer: function BreadcrumbTrail_getContainer() {
    return this._containerElement;
  },
  /**
   * Add a breadcrumb. The breadcrumb parameter is an object with the following
   * properties:
   *  - title: The text that will be shown in the breadcrumb's button.
   *  - enterCallback: A function that will be called when entering this
   *                   breadcrumb.
   */
  add: function BreadcrumbTrail_add(breadcrumb) {
    for (var i = this._breadcrumbs.length - 1; i > this._selectedBreadcrumbIndex; i--) {
      var rearLi = this._breadcrumbs[i];
      if (!rearLi.breadcrumbIsTransient)
        throw "Can only add new breadcrumbs if after the current one there are only transient ones.";
      rearLi.breadcrumbDiscarder.discard();
    }
    var div = document.createElement("div");
    div.className = "breadcrumbTrailItem";
    div.textContent = breadcrumb.title;
    var index = this._breadcrumbs.length;
    div.breadcrumbIndex = index;
    div.breadcrumbEnterCallback = breadcrumb.enterCallback;
    div.breadcrumbIsTransient = true;
    div.style.zIndex = 1000 - index;
    this._containerElement.appendChild(div);
    this._breadcrumbs.push(div);
    if (index == 0)
      this._enter(index);
    var self = this;
    div.breadcrumbDiscarder = {
      discard: function () {
        if (div.breadcrumbIsTransient) {
          self._deleteBeyond(index - 1);
          delete div.breadcrumbIsTransient;
          delete div.breadcrumbDiscarder;
        }
      }
    };
    return div.breadcrumbDiscarder;
  },
  addAndEnter: function BreadcrumbTrail_addAndEnter(breadcrumb) {
    var removalHandle = this.add(breadcrumb);
    this._enter(this._breadcrumbs.length - 1);
  },
  pop : function BreadcrumbTrail_pop() {
    if (this._breadcrumbs.length-2 >= 0)
      this._enter(this._breadcrumbs.length-2);
  },
  _enter: function BreadcrumbTrail__select(index) {
    if (index == this._selectedBreadcrumbIndex)
      return;
    gTreeManager.saveSelectionSnapshot();
    var prevSelected = this._breadcrumbs[this._selectedBreadcrumbIndex];
    if (prevSelected)
      prevSelected.classList.remove("selected");
    var li = this._breadcrumbs[index];
    if (this === gBreadcrumbTrail && index != 0) {
      // Support for back button, disabled until the forward button is implemented.
      //var state = {action: "popbreadcrumb",};
      //window.history.pushState(state, "Cleopatra");
    }
    if (!li)
      console.log("li at index " + index + " is null!");
    delete li.breadcrumbIsTransient;
    li.classList.add("selected");
    this._deleteBeyond(index);
    this._selectedBreadcrumbIndex = index;
    li.breadcrumbEnterCallback();
    // Add history state
  },
  _deleteBeyond: function BreadcrumbTrail__deleteBeyond(index) {
    while (this._breadcrumbs.length > index + 1) {
      this._hide(this._breadcrumbs[index + 1]);
      this._breadcrumbs.splice(index + 1, 1);
    }
  },
  _hide: function BreadcrumbTrail__hide(breadcrumb) {
    delete breadcrumb.breadcrumbIsTransient;
    breadcrumb.classList.add("deleted");
    setTimeout(function () {
      breadcrumb.parentNode.removeChild(breadcrumb);
    }, 1000);
  },
};

function maxResponsiveness() {
  var data = gCurrentlyShownSampleData;
  var maxRes = 0.0;
  for (var i = 0; i < data.length; ++i) {
    if (!data[i] || !data[i].extraInfo || !data[i].extraInfo["responsiveness"])
      continue;
    if (maxRes < data[i].extraInfo["responsiveness"])
      maxRes = data[i].extraInfo["responsiveness"];
  }
  return maxRes;
}

function effectiveInterval() {
  var data = gCurrentlyShownSampleData;
  var interval = 0.0;
  var sampleCount = 0;
  var timeCount = 0;
  var lastTime = null;
  for (var i = 0; i < data.length; ++i) {
    if (!data[i] || !data[i].extraInfo || !data[i].extraInfo["time"]) {
      lastTime = null;
      continue;
    }
    if (lastTime) {
      sampleCount++;
      timeCount += data[i].extraInfo["time"] - lastTime;
    }
    lastTime = data[i].extraInfo["time"];
  }
  var effectiveInterval = timeCount/sampleCount;
  // Biggest diff
  var biggestDiff = 0;
  lastTime = null;
  for (var i = 0; i < data.length; ++i) {
    if (!data[i] || !data[i].extraInfo || !data[i].extraInfo["time"]) {
      lastTime = null;
      continue;
    }
    if (lastTime) {
      if (biggestDiff < Math.abs(effectiveInterval - (data[i].extraInfo["time"] - lastTime)))
        biggestDiff = Math.abs(effectiveInterval - (data[i].extraInfo["time"] - lastTime));
    }
    lastTime = data[i].extraInfo["time"];
  }

  if (effectiveInterval != effectiveInterval)
    return "Time info not collected";

  return (effectiveInterval).toFixed(2) + " ms ±" + biggestDiff.toFixed(2);
}

function numberOfCurrentlyShownSamples() {
  var data = gCurrentlyShownSampleData;
  var num = 0;
  for (var i = 0; i < data.length; ++i) {
    if (data[i])
      num++;
  }
  return num;
}

function avgResponsiveness() {
  var data = gCurrentlyShownSampleData;
  var totalRes = 0.0;
  for (var i = 0; i < data.length; ++i) {
    if (!data[i] || !data[i].extraInfo || !data[i].extraInfo["responsiveness"])
      continue;
    totalRes += data[i].extraInfo["responsiveness"];
  }
  return totalRes / numberOfCurrentlyShownSamples();
}

function copyProfile() {
  window.prompt ("Copy to clipboard: Ctrl+C, Enter", document.getElementById("data").value);
}

function saveProfileToLocalStorage() {
  Parser.getSerializedProfile(true, function (serializedProfile) {
    gLocalStorage.storeLocalProfile(serializedProfile, function profileSaved() {

    });
  });
}
function downloadProfile() {
  Parser.getSerializedProfile(true, function (serializedProfile) {
    var blob = new Blob([serializedProfile], { "type": "application/octet-stream" });
    location.href = window.URL.createObjectURL(blob);
  });
}

function uploadProfile(selected) {
  Parser.getSerializedProfile(!selected, function (dataToUpload) {
    var oXHR = new XMLHttpRequest();
    oXHR.open("POST", "http://profile-store.appspot.com/store", true);
    oXHR.onload = function (oEvent) {
      if (oXHR.status == 200) {
        document.getElementById("upload_status").innerHTML = "Success! Use this <a href='" + document.URL.split('?')[0] + "?report=" + oXHR.responseText + "'>link</a>";
      } else {
        document.getElementById("upload_status").innerHTML = "Error " + oXHR.status + " occurred uploading your file.";
      }
    };
    oXHR.onerror = function (oEvent) {
      document.getElementById("upload_status").innerHTML = "Error " + oXHR.status + " occurred uploading your file.";
    }
    oXHR.onprogress = function (oEvent) {
      if (oEvent.lengthComputable) {
        document.getElementById("upload_status").innerHTML = "Uploading: " + ((oEvent.loaded / oEvent.total)*100) + "%";
      }
    }

    var dataSize;
    if (dataToUpload.length > 1024*1024) {
      dataSize = (dataToUpload.length/1024/1024) + " MB(s)";
    } else {
      dataSize = (dataToUpload.length/1024) + " KB(s)";
    }

    var formData = new FormData();
    formData.append("file", dataToUpload);
    document.getElementById("upload_status").innerHTML = "Uploading Profile (" + dataSize + ")";
    oXHR.send(formData);
  });
}

function populate_skip_symbol() {
  var skipSymbolCtrl = document.getElementById('skipsymbol')
  //skipSymbolCtrl.options = gSkipSymbols;
  for (var i = 0; i < gSkipSymbols.length; i++) {
    var elOptNew = document.createElement('option');
    elOptNew.text = gSkipSymbols[i];
    elOptNew.value = gSkipSymbols[i];
    elSel.add(elOptNew);
  }

}

function delete_skip_symbol() {
  var skipSymbol = document.getElementById('skipsymbol').value
}

function add_skip_symbol() {

}

var gFilterChangeCallback = null;
var gFilterChangeDelay = 1200;
function filterOnChange() {
  if (gFilterChangeCallback != null) {
    clearTimeout(gFilterChangeCallback);
    gFilterChangeCallback = null;
  }

  gFilterChangeCallback = setTimeout(filterUpdate, gFilterChangeDelay);
}
function filterUpdate() {
  gFilterChangeCallback = null;

  filtersChanged();

  var filterNameInput = document.getElementById("filterName");
  if (filterNameInput != null) {
    filterNameInput.focus();
  }
}

// Maps document id to a tooltip description
var tooltip = {
  "mergeFunctions" : "Ignore line information and merge samples based on function names.",
  "showJank" : "Show only samples with >50ms responsiveness.",
  "showJS" : "Show only samples which involve running chrome or content Javascript code.",
  "mergeUnbranched" : "Collapse unbranched call paths in the call tree into a single node.",
  "filterName" : "Show only samples with a frame containing the filter as a substring.",
  "invertCallstack" : "Invert the callstack (Heavy view) to find the most expensive leaf functions.",
  "upload" : "Upload the full profile to public cloud storage to share with others.",
  "upload_select" : "Upload only the selected view.",
  "download" : "Initiate a download of the full profile.",
}

function addTooltips() {
  for (var elemId in tooltip) {
    var elem = document.getElementById(elemId);
    if (!elem)
      continue;
    if (elem.parentNode.nodeName.toLowerCase() == "label")
      elem = elem.parentNode;
    elem.title = tooltip[elemId];
  }
}

function InfoBar() {
  this._container = document.createElement("div");
  this._container.id = "infoBar";
  this._container.className = "sideBar";
}

InfoBar.prototype = {
  getContainer: function InfoBar_getContainer() {
    return this._container;
  },
  display: function InfoBar_display() {
    function getMetaFeatureString() {
      features = "<dt>Stackwalk:</dt><dd>" + (gMeta.stackwalk ? "True" : "False") + "</dd>";
      features += "<dt>Jank:</dt><dd>" + (gMeta.stackwalk ? "True" : "False") + "</dd>";
      return features;
    }
    function getPlatformInfo() {
      return gMeta.oscpu + " (" + gMeta.toolkit + ")";
    }
    var infobar = this._container;
    var infoText = "";

    if (gMeta) {
      infoText += "<h2>Profile Info</h2>\n<dl>\n";
      infoText += "<dt>Product:</dt><dd>" + gMeta.product + "</dd>";
      infoText += "<dt>Platform:</dt><dd>" + getPlatformInfo() + "</dd>";
      infoText += getMetaFeatureString();
      infoText += "<dt>Interval:</dt><dd>" + gMeta.interval + " ms</dd></dl>";
    }
    infoText += "<h2>Selection Info</h2>\n<dl>\n";
    infoText += "  <dt>Avg. Responsiveness:</dt><dd>" + avgResponsiveness().toFixed(2) + " ms</dd>\n";
    infoText += "  <dt>Max Responsiveness:</dt><dd>" + maxResponsiveness().toFixed(2) + " ms</dd>\n";
    infoText += "  <dt>Real Interval:</dt><dd>" + effectiveInterval() + "</dd>";
    infoText += "</dl>\n";
    infoText += "<h2>Pre Filtering</h2>\n";
    // Disable for now since it's buggy and not useful
    //infoText += "<label><input type='checkbox' id='mergeFunctions' " + (gMergeFunctions ?" checked='true' ":" ") + " onchange='toggleMergeFunctions()'/>Functions, not lines</label><br>\n";
    infoText += "<label><input type='checkbox' id='showJS' " + (gJavascriptOnly ?" checked='true' ":" ") + " onchange='toggleJavascriptOnly()'/>Javascript only</label><br>\n";

    var filterNameInputOld = document.getElementById("filterName");
    infoText += "<label>Filter:\n";
    infoText += "<input type='search' id='filterName' oninput='filterOnChange()'/></label>\n";

    infoText += "<h2>Post Filtering</h2>\n";
    infoText += "<label><input type='checkbox' id='showJank' " + (gJankOnly ?" checked='true' ":" ") + " onchange='toggleJank()'/>Show Jank only</label>\n";
    infoText += "<h2>View Options</h2>\n";
    infoText += "<label><input type='checkbox' id='mergeUnbranched' " + (gMergeUnbranched ?" checked='true' ":" ") + " onchange='toggleMergeUnbranched()'/>Merge unbranched call paths</label><br>\n";
    infoText += "<label><input type='checkbox' id='invertCallstack' " + (gInvertCallstack ?" checked='true' ":" ") + " onchange='toggleInvertCallStack()'/>Invert callstack</label><br>\n";

    infoText += "<h2>Share</h2>\n";
    infoText += "<div id='upload_status' aria-live='polite'>No upload in progress</div><br>\n";
    infoText += "<input type='button' id='upload' value='Upload full profile'>\n";
    infoText += "<input type='button' id='upload_select' value='Upload view'><br>\n";
    infoText += "<input type='button' id='download' value='Download full profile'>\n";

    //infoText += "<br>\n";
    //infoText += "Skip functions:<br>\n";
    //infoText += "<select size=8 id='skipsymbol'></select><br />"
    //infoText += "<input type='button' id='delete_skipsymbol' value='Delete'/><br />\n";
    //infoText += "<input type='button' id='add_skipsymbol' value='Add'/><br />\n";

    infobar.innerHTML = infoText;
    addTooltips();

    var filterNameInputNew = document.getElementById("filterName");
    if (filterNameInputOld != null && filterNameInputNew != null) {
      filterNameInputNew.parentNode.replaceChild(filterNameInputOld, filterNameInputNew);
      //filterNameInputNew.value = filterNameInputOld.value;
    }
    document.getElementById('upload').onclick = function() {
      uploadProfile(false);
    };
    document.getElementById('download').onclick = downloadProfile;
    document.getElementById('upload_select').onclick = function() {
      uploadProfile(true);
    };
    //document.getElementById('delete_skipsymbol').onclick = delete_skip_symbol;
    //document.getElementById('add_skipsymbol').onclick = add_skip_symbol;

    //populate_skip_symbol();
  }
}

// in light mode we simplify the UI by default
var gLightMode = false;
var gNumSamples = 0;
var gMeta = null;
var gSymbols = {};
var gFunctions = {};
var gHighlightedCallstack = [];
var gTreeManager = null;
var gSampleBar = null;
var gBreadcrumbTrail = null;
var gHistogramView = null;
var gDiagnosticBar = null;
var gVideoPane = null;
var gPluginView = null;
var gFileList = null;
var gInfoBar = null;
var gMainArea = null;
var gCurrentlyShownSampleData = null;
var gSkipSymbols = ["test2", "test1"];
var gAppendVideoCapture = null;

function getTextData() {
  var data = [];
  var samples = gCurrentlyShownSampleData;
  for (var i = 0; i < samples.length; i++) {
    data.push(samples[i].lines.join("\n"));
  }
  return data.join("\n");
}

function loadProfileFile(fileList) {
  if (fileList.length == 0)
    return;
  var file = fileList[0];
  var reporter = enterProgressUI();
  var subreporters = reporter.addSubreporters({
    fileLoading: 1000,
    parsing: 1000
  });

  var reader = new FileReader();
  reader.onloadend = function () {
    subreporters.fileLoading.finish();
    loadRawProfile(subreporters.parsing, reader.result);
  };
  reader.onprogress = function (e) {
    subreporters.fileLoading.setProgress(e.loaded / e.total);
  };
  reader.readAsText(file, "utf-8");
  subreporters.fileLoading.begin("Reading local file...");
}

function loadLocalStorageProfile(profileKey) {
  var reporter = enterProgressUI();
  var subreporters = reporter.addSubreporters({
    fileLoading: 1000,
    parsing: 1000
  });

  gLocalStorage.getProfile(profileKey, function(profile) {
    subreporters.fileLoading.finish();
    loadRawProfile(subreporters.parsing, JSON.stringify(profile));
  });
  subreporters.fileLoading.begin("Reading local storage...");
}

function appendVideoCapture(videoCapture) {
  if (videoCapture.indexOf("://") == -1) {
    videoCapture = EIDETICKER_BASE_URL + videoCapture;
  }
  gAppendVideoCapture = videoCapture;
}

function loadZippedProfileURL(url) {
  var reporter = enterProgressUI();
  var subreporters = reporter.addSubreporters({
    fileLoading: 1000,
    parsing: 1000
  });

  // Crude way to detect if we're using a relative URL or not :(
  if (url.indexOf("://") == -1) {
    url = EIDETICKER_BASE_URL + url;
  }
  reporter.begin("Fetching " + url);
  PROFILERTRACE("Fetch url: " + url);

  function onerror(e) {
    PROFILERERROR("zip.js error");
    PROFILERERROR(JSON.stringify(e));
  }

  zip.workerScriptsPath = "js/zip.js/";
  zip.createReader(new zip.HttpReader(url), function(zipReader) {
    subreporters.fileLoading.setProgress(0.4);
    zipReader.getEntries(function(entries) {
      for (var i = 0; i < entries.length; i++) {
        var entry = entries[i];
        PROFILERTRACE("Zip file: " + entry.filename);
        if (entry.filename === "symbolicated_profile.txt") {
          reporter.begin("Decompressing " + url);
          subreporters.fileLoading.setProgress(0.8);
          entry.getData(new zip.TextWriter(), function(profileText) {
            subreporters.fileLoading.finish();
            loadRawProfile(subreporters.parsing, profileText);
          });
          return;
        }
        onerror("symbolicated_profile.txt not found in zip file.");
      }
    });
  }, onerror);
}

function loadProfileURL(url) {
  var reporter = enterProgressUI();
  var subreporters = reporter.addSubreporters({
    fileLoading: 1000,
    parsing: 1000
  });

  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.responseType = "text";
  xhr.onreadystatechange = function (e) {
    if (xhr.readyState === 4 && xhr.status === 200) {
      subreporters.fileLoading.finish();
      PROFILERLOG("Got profile from '" + url + "'.");
      loadRawProfile(subreporters.parsing, xhr.responseText);
    }
  };
  xhr.onerror = function (e) {
    subreporters.fileLoading.begin("Error fetching profile :(. URL:  " + url);
  }
  xhr.onprogress = function (e) {
    if (e.lengthComputable && (e.loaded <= e.total)) {
      subreporters.fileLoading.setProgress(e.loaded / e.total);
    } else {
      subreporters.fileLoading.setProgress(NaN);
    }
  };
  xhr.send(null);
  subreporters.fileLoading.begin("Loading remote file...");
}

function loadProfile(rawProfile) {
  if (!rawProfile)
    return;
  var reporter = enterProgressUI();
  loadRawProfile(reporter, rawProfile);
}

function loadRawProfile(reporter, rawProfile) {
  PROFILERLOG("Parse raw profile: ~" + rawProfile.length + " bytes");
  reporter.begin("Parsing...");
  var startTime = Date.now();
  var parseRequest = Parser.parse(rawProfile, {
    appendVideoCapture : gAppendVideoCapture,
  });
  gVideoCapture = null;
  parseRequest.addEventListener("progress", function (progress, action) {
    if (action)
      reporter.setAction(action);
    reporter.setProgress(progress);
  });
  parseRequest.addEventListener("finished", function (result) {
    console.log("parsing (in worker): " + (Date.now() - startTime) + "ms");
    reporter.finish();
    gMeta = result.meta;
    gNumSamples = result.numSamples;
    gSymbols = result.symbols;
    gFunctions = result.functions;
    enterFinishedProfileUI();
    if (gFileList)
      gFileList.profileParsingFinished();
  });
}

var gImportFromAddonSubreporters = null;

window.addEventListener("message", function messageFromAddon(msg) {
  // This is triggered by the profiler add-on.
  var o = JSON.parse(msg.data);
  switch (o.task) {
    case "importFromAddonStart":
      var totalReporter = enterProgressUI();
      gImportFromAddonSubreporters = totalReporter.addSubreporters({
        import: 10000,
        parsing: 1000
      });
      gImportFromAddonSubreporters.import.begin("Symbolicating...");
      break;
    case "importFromAddonProgress":
      gImportFromAddonSubreporters.import.setProgress(o.progress);
      if (o.action != null) {
          gImportFromAddonSubreporters.import.setAction(o.action);
      }
      break;
    case "importFromAddonFinish":
      importFromAddonFinish(o.rawProfile);
      break;
    case "receiveProfileData":
      receiveProfileData(o.rawProfile);
      break;
  }
});

function receiveProfileData(data) {
 loadProfile(JSON.stringify(data));
}

function importFromAddonFinish(rawProfile) {
  gImportFromAddonSubreporters.import.finish();
  loadRawProfile(gImportFromAddonSubreporters.parsing, rawProfile);
}

var gInvertCallstack = false;
function toggleInvertCallStack() {
  gTreeManager.saveReverseSelectionSnapshot(gJavascriptOnly);
  gInvertCallstack = !gInvertCallstack;
  var startTime = Date.now();
  viewOptionsChanged();
  console.log("invert time: " + (Date.now() - startTime) + "ms");
}

var gMergeUnbranched = false;
function toggleMergeUnbranched() {
  gMergeUnbranched = !gMergeUnbranched;
  viewOptionsChanged();
}

var gMergeFunctions = true;
function toggleMergeFunctions() {
  gMergeFunctions = !gMergeFunctions;
  filtersChanged();
}

var gJankOnly = false;
var gJankThreshold = 50 /* ms */;
function toggleJank(/* optional */ threshold) {
  // Currently we have no way to change the threshold in the UI
  // once we add this we will need to change the tooltip.
  gJankOnly = !gJankOnly;
  if (threshold != null ) {
    gJankThreshold = threshold;
  }
  filtersChanged();
}

var gJavascriptOnly = false;
function toggleJavascriptOnly() {
  if (gJavascriptOnly) {
    // When going from JS only to non js there's going to be new C++
    // frames in the selection so we need to restore the selection
    // while allowing non contigous symbols to be in the stack (the c++ ones)
    gTreeManager.setAllowNonContigous();
  }
  gJavascriptOnly = !gJavascriptOnly;
  gTreeManager.saveSelectionSnapshot(gJavascriptOnly);
  filtersChanged();
}

var gSampleFilters = [];
function focusOnSymbol(focusSymbol, name) {
  var newFilterChain = gSampleFilters.concat([{type: "FocusedFrameSampleFilter", focusedSymbol: focusSymbol}]);
  gBreadcrumbTrail.addAndEnter({
    title: name,
    enterCallback: function () {
      gSampleFilters = newFilterChain;
      filtersChanged();
    }
  });
}

function focusOnCallstack(focusedCallstack, name) {
  var filter = {
    type: gInvertCallstack ? "FocusedCallstackPostfixSampleFilter" : "FocusedCallstackPrefixSampleFilter",
    focusedCallstack: focusedCallstack
  };
  var newFilterChain = gSampleFilters.concat([filter]);
  gBreadcrumbTrail.addAndEnter({
    title: name,
    enterCallback: function () {
      gSampleFilters = newFilterChain;
      filtersChanged();
    }
  })
}

function focusOnPluginView(pluginName, param) {
  var filter = {
    type: "PluginView",
    pluginName: pluginName,
    param: param,
  };
  var newFilterChain = gSampleFilters.concat([filter]);
  gBreadcrumbTrail.addAndEnter({
    title: "Plugin View: " + pluginName,
    enterCallback: function () {
      gSampleFilters = newFilterChain;
      filtersChanged();
    }
  })
}

function viewJSSource(sample) {
  var sourceView = new SourceView();
  sourceView.setScriptLocation(sample.scriptLocation);
  sourceView.setSource(gMeta.js.source[sample.scriptLocation.scriptURI]);
  gMainArea.appendChild(sourceView.getContainer());

}

function setHighlightedCallstack(samples, heaviestSample) {
  PROFILERTRACE("highlight: " + samples);
  gHighlightedCallstack = samples;
  gHistogramView.highlightedCallstackChanged(gHighlightedCallstack);
  if (!gInvertCallstack) {
    // Always show heavy
    heaviestSample = heaviestSample.clone().reverse();
  }
  if (gSampleBar)
    gSampleBar.setSample(heaviestSample);
}

function enterMainUI(isLightMode) {
  if (isLightMode !== undefined) {
    gLightMode = isLightMode;
    if (gLightMode) {
      gJavascriptOnly = true;
    }
  }

  var uiContainer = document.createElement("div");
  uiContainer.id = "ui";

  //gFileList.loadProfileListFromLocalStorage();
  if (!gLightMode) {
    gFileList = new FileList();
    uiContainer.appendChild(gFileList.getContainer());

    gFileList.addFile();
    gInfoBar = new InfoBar();
    uiContainer.appendChild(gInfoBar.getContainer());
  }

  gMainArea = document.createElement("div");
  gMainArea.id = "mainarea";
  uiContainer.appendChild(gMainArea);
  document.body.appendChild(uiContainer);

  var profileEntryPane = document.createElement("div");
  profileEntryPane.className = "profileEntryPane";
  profileEntryPane.innerHTML = '' +
    '<h1>Upload your profile here:</h1>' +
    '<input type="file" id="datafile" onchange="loadProfileFile(this.files);">' +
    '<h1>Or, alternatively, enter your profile data here:</h1>' +
    '<textarea rows=20 cols=80 id=data autofocus spellcheck=false></textarea>' +
    '<p><button onclick="loadProfile(document.getElementById(\'data\').value);">Parse</button></p>' +
    '';

  gMainArea.appendChild(profileEntryPane);
}

function enterProgressUI() {
  var profileProgressPane = document.createElement("div");
  profileProgressPane.className = "profileProgressPane";

  var progressLabel = document.createElement("a");
  profileProgressPane.appendChild(progressLabel);

  var progressBar = document.createElement("progress");
  profileProgressPane.appendChild(progressBar);

  var totalProgressReporter = new ProgressReporter();
  totalProgressReporter.addListener(function (r) {
    var progress = r.getProgress();
    progressLabel.innerHTML = r.getAction();
    console.log("Action: " + r.getAction());
    if (isNaN(progress))
      progressBar.removeAttribute("value");
    else
      progressBar.value = progress;
  });

  gMainArea.appendChild(profileProgressPane);

  Parser.updateLogSetting();

  return totalProgressReporter;
}

function enterFinishedProfileUI() {
  //dump("prepare to save\n");
  //saveProfileToLocalStorage();
  //dump("prepare to saved\n");

  var finishedProfilePaneBackgroundCover = document.createElement("div");
  finishedProfilePaneBackgroundCover.className = "finishedProfilePaneBackgroundCover";

  var finishedProfilePane = document.createElement("table");
  var rowIndex = 0;
  var currRow;
  finishedProfilePane.style.width = "100%";
  finishedProfilePane.style.height = "100%";
  finishedProfilePane.border = "0";
  finishedProfilePane.cellPadding = "0";
  finishedProfilePane.cellSpacing = "0";
  finishedProfilePane.borderCollapse = "collapse";
  finishedProfilePane.className = "finishedProfilePane";
  setTimeout(function() {
    // Work around a webkit bug. For some reason the table doesn't show up
    // until some actions happen such as focusing this box
    var filterNameInput = document.getElementById("filterName");
    if (filterNameInput != null) {
      filterNameInput.focus();
     }
  }, 100);

  gBreadcrumbTrail = new BreadcrumbTrail();
  currRow = finishedProfilePane.insertRow(rowIndex++);
  currRow.insertCell(0).appendChild(gBreadcrumbTrail.getContainer());

  gHistogramView = new HistogramView();
  currRow = finishedProfilePane.insertRow(rowIndex++);
  currRow.insertCell(0).appendChild(gHistogramView.getContainer());

  if (typeof DiagnosticBar !== "undefined") {
    gDiagnosticBar = new DiagnosticBar();
    gDiagnosticBar.setDetailsListener(function(details) {
      if (details.indexOf("bug ") == 0) {
        window.open('https://bugzilla.mozilla.org/show_bug.cgi?id=' + details.substring(4));
      } else {
        var sourceView = new SourceView();
        sourceView.setText("Diagnostic", js_beautify(details));
        gMainArea.appendChild(sourceView.getContainer());
      }
    });
    currRow = finishedProfilePane.insertRow(rowIndex++);
    currRow.insertCell(0).appendChild(gDiagnosticBar.getContainer());
  }

  // For testing:
  //gMeta.videoCapture = {
  //  src: "http://videos-cdn.mozilla.net/brand/Mozilla_Firefox_Manifesto_v0.2_640.webm",
  //};

  if (!gLightMode && gMeta && gMeta.videoCapture) {
    gVideoPane = new VideoPane(gMeta.videoCapture);
    gVideoPane.onTimeChange(videoPaneTimeChange);
    currRow = finishedProfilePane.insertRow(rowIndex++);
    currRow.insertCell(0).appendChild(gVideoPane.getContainer());
  }

  var treeContainerDiv = document.createElement("div");
  treeContainerDiv.className = "treeContainer";
  treeContainerDiv.style.width = "100%";
  treeContainerDiv.style.height = "100%";

  gTreeManager = new ProfileTreeManager();
  currRow = finishedProfilePane.insertRow(rowIndex++);
  currRow.style.height = "100%";
  var cell = currRow.insertCell(0);
  cell.appendChild(treeContainerDiv);
  treeContainerDiv.appendChild(gTreeManager.getContainer());

  if (!gLightMode) {
    gSampleBar = new SampleBar();
    treeContainerDiv.appendChild(gSampleBar.getContainer());
  }

  // sampleBar

  if (!gLightMode) {
    gPluginView = new PluginView();
    //currRow = finishedProfilePane.insertRow(4);
    treeContainerDiv.appendChild(gPluginView.getContainer());
  }

  gMainArea.appendChild(finishedProfilePaneBackgroundCover);
  gMainArea.appendChild(finishedProfilePane);

  gBreadcrumbTrail.add({
    title: "Complete Profile",
    enterCallback: function () {
      gSampleFilters = [];
      filtersChanged();
    }
  });
}

function filtersChanged() {
  var data = { symbols: {}, functions: {}, samples: [] };

  gHistogramView.dataIsOutdated();
  var filterNameInput = document.getElementById("filterName");
  var updateRequest = Parser.updateFilters({
    mergeFunctions: gMergeFunctions,
    nameFilter: (filterNameInput && filterNameInput.value) || "",
    sampleFilters: gSampleFilters,
    jankOnly: gJankOnly,
    javascriptOnly: gJavascriptOnly
  });
  var start = Date.now();
  updateRequest.addEventListener("finished", function (filteredSamples) {
    console.log("profile filtering (in worker): " + (Date.now() - start) + "ms.");
    gCurrentlyShownSampleData = filteredSamples;
    if (gInfoBar)
      gInfoBar.display();

    if (gPluginView && gSampleFilters.length > 0 && gSampleFilters[gSampleFilters.length-1].type === "PluginView") {
      start = Date.now();
      gPluginView.display(gSampleFilters[gSampleFilters.length-1].pluginName, gSampleFilters[gSampleFilters.length-1].param,
                          gCurrentlyShownSampleData, gHighlightedCallstack);
      console.log("plugin displaying: " + (Date.now() - start) + "ms.");
    } else if (gPluginView) {
      gPluginView.hide();
    }
  });

  var histogramRequest = Parser.calculateHistogramData();
  histogramRequest.addEventListener("finished", function (data) {
    start = Date.now();
    gHistogramView.display(data.histogramData, data.frameStart, data.widthSum, gHighlightedCallstack);
    console.log("histogram displaying: " + (Date.now() - start) + "ms.");
  });

  if (gDiagnosticBar) {
    var diagnosticsRequest = Parser.calculateDiagnosticItems(gMeta);
    diagnosticsRequest.addEventListener("finished", function (diagnosticItems) {
      start = Date.now();
      gDiagnosticBar.display(diagnosticItems);
      console.log("diagnostic items displaying: " + (Date.now() - start) + "ms.");
    });
  }

  viewOptionsChanged();
}

function viewOptionsChanged() {
  gTreeManager.dataIsOutdated();
  var filterNameInput = document.getElementById("filterName");
  var updateViewOptionsRequest = Parser.updateViewOptions({
    invertCallstack: gInvertCallstack,
    mergeUnbranched: gMergeUnbranched
  });
  updateViewOptionsRequest.addEventListener("finished", function (calltree) {
    var start = Date.now();
    gTreeManager.display(calltree, gSymbols, gFunctions, gMergeFunctions, filterNameInput && filterNameInput.value);
    console.log("tree displaying: " + (Date.now() - start) + "ms.");
  });
}
