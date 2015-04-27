/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling details about a single recorded animation frame snapshot
 * (the calls list, rendering preview, thumbnails filmstrip etc.).
 */
let CallsListView = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the tool is started.
   */
  initialize: function() {
    this.widget = new SideMenuWidget($("#calls-list"));
    this._slider = $("#calls-slider");
    this._searchbox = $("#calls-searchbox");
    this._filmstrip = $("#snapshot-filmstrip");

    this._onSelect = this._onSelect.bind(this);
    this._onSlideMouseDown = this._onSlideMouseDown.bind(this);
    this._onSlideMouseUp = this._onSlideMouseUp.bind(this);
    this._onSlide = this._onSlide.bind(this);
    this._onSearch = this._onSearch.bind(this);
    this._onScroll = this._onScroll.bind(this);
    this._onExpand = this._onExpand.bind(this);
    this._onStackFileClick = this._onStackFileClick.bind(this);
    this._onThumbnailClick = this._onThumbnailClick.bind(this);

    this.widget.addEventListener("select", this._onSelect, false);
    this._slider.addEventListener("mousedown", this._onSlideMouseDown, false);
    this._slider.addEventListener("mouseup", this._onSlideMouseUp, false);
    this._slider.addEventListener("change", this._onSlide, false);
    this._searchbox.addEventListener("input", this._onSearch, false);
    this._filmstrip.addEventListener("wheel", this._onScroll, false);
  },

  /**
   * Destruction function, called when the tool is closed.
   */
  destroy: function() {
    this.widget.removeEventListener("select", this._onSelect, false);
    this._slider.removeEventListener("mousedown", this._onSlideMouseDown, false);
    this._slider.removeEventListener("mouseup", this._onSlideMouseUp, false);
    this._slider.removeEventListener("change", this._onSlide, false);
    this._searchbox.removeEventListener("input", this._onSearch, false);
    this._filmstrip.removeEventListener("wheel", this._onScroll, false);
  },

  /**
   * Populates this container with a list of function calls.
   *
   * @param array functionCalls
   *        A list of function call actors received from the backend.
   */
  showCalls: function(functionCalls) {
    this.empty();

    for (let i = 0, len = functionCalls.length; i < len; i++) {
      let call = functionCalls[i];

      let view = document.createElement("vbox");
      view.className = "call-item-view devtools-monospace";
      view.setAttribute("flex", "1");

      let contents = document.createElement("hbox");
      contents.className = "call-item-contents";
      contents.setAttribute("align", "center");
      contents.addEventListener("dblclick", this._onExpand);
      view.appendChild(contents);

      let index = document.createElement("label");
      index.className = "plain call-item-index";
      index.setAttribute("flex", "1");
      index.setAttribute("value", i + 1);

      let gutter = document.createElement("hbox");
      gutter.className = "call-item-gutter";
      gutter.appendChild(index);
      contents.appendChild(gutter);

      // Not all function calls have a caller that was stringified (e.g.
      // context calls have a "gl" or "ctx" caller preview).
      if (call.callerPreview) {
        let context = document.createElement("label");
        context.className = "plain call-item-context";
        context.setAttribute("value", call.callerPreview);
        contents.appendChild(context);

        let separator = document.createElement("label");
        separator.className = "plain call-item-separator";
        separator.setAttribute("value", ".");
        contents.appendChild(separator);
      }

      let name = document.createElement("label");
      name.className = "plain call-item-name";
      name.setAttribute("value", call.name);
      contents.appendChild(name);

      let argsPreview = document.createElement("label");
      argsPreview.className = "plain call-item-args";
      argsPreview.setAttribute("crop", "end");
      argsPreview.setAttribute("flex", "100");
      // Getters and setters are displayed differently from regular methods.
      if (call.type == CallWatcherFront.METHOD_FUNCTION) {
        argsPreview.setAttribute("value", "(" + call.argsPreview + ")");
      } else {
        argsPreview.setAttribute("value", " = " + call.argsPreview);
      }
      contents.appendChild(argsPreview);

      let location = document.createElement("label");
      location.className = "plain call-item-location";
      location.setAttribute("value", getFileName(call.file) + ":" + call.line);
      location.setAttribute("crop", "start");
      location.setAttribute("flex", "1");
      location.addEventListener("mousedown", this._onExpand);
      contents.appendChild(location);

      // Append a function call item to this container.
      this.push([view], {
        staged: true,
        attachment: {
          actor: call
        }
      });

      // Highlight certain calls that are probably more interesting than
      // everything else, making it easier to quickly glance over them.
      if (CanvasFront.DRAW_CALLS.has(call.name)) {
        view.setAttribute("draw-call", "");
      }
      if (CanvasFront.INTERESTING_CALLS.has(call.name)) {
        view.setAttribute("interesting-call", "");
      }
    }

    // Flushes all the prepared function call items into this container.
    this.commit();
    window.emit(EVENTS.CALL_LIST_POPULATED);

    // Resetting the function selection slider's value (shown in this
    // container's toolbar) would trigger a selection event, which should be
    // ignored in this case.
    this._ignoreSliderChanges = true;
    this._slider.value = 0;
    this._slider.max = functionCalls.length - 1;
    this._ignoreSliderChanges = false;
  },

  /**
   * Displays an image in the rendering preview of this container, generated
   * for the specified draw call in the recorded animation frame snapshot.
   *
   * @param array screenshot
   *        A single "snapshot-image" instance received from the backend.
   */
  showScreenshot: function(screenshot) {
    let { index, width, height, scaling, flipped, pixels } = screenshot;

    let screenshotNode = $("#screenshot-image");
    screenshotNode.setAttribute("flipped", flipped);
    drawBackground("screenshot-rendering", width, height, pixels);

    let dimensionsNode = $("#screenshot-dimensions");
    let actualWidth = (width / scaling) | 0;
    let actualHeight = (height / scaling) | 0;
    dimensionsNode.setAttribute("value",
      SHARED_L10N.getFormatStr("dimensions", actualWidth, actualHeight));

    window.emit(EVENTS.CALL_SCREENSHOT_DISPLAYED);
  },

  /**
   * Populates this container's footer with a list of thumbnails, one generated
   * for each draw call in the recorded animation frame snapshot.
   *
   * @param array thumbnails
   *        An array of "snapshot-image" instances received from the backend.
   */
  showThumbnails: function(thumbnails) {
    while (this._filmstrip.hasChildNodes()) {
      this._filmstrip.firstChild.remove();
    }
    for (let thumbnail of thumbnails) {
      this.appendThumbnail(thumbnail);
    }

    window.emit(EVENTS.THUMBNAILS_DISPLAYED);
  },

  /**
   * Displays an image in the thumbnails list of this container, generated
   * for the specified draw call in the recorded animation frame snapshot.
   *
   * @param array thumbnail
   *        A single "snapshot-image" instance received from the backend.
   */
  appendThumbnail: function(thumbnail) {
    let { index, width, height, flipped, pixels } = thumbnail;

    let thumbnailNode = document.createElementNS(HTML_NS, "canvas");
    thumbnailNode.setAttribute("flipped", flipped);
    thumbnailNode.width = Math.max(CanvasFront.THUMBNAIL_SIZE, width);
    thumbnailNode.height = Math.max(CanvasFront.THUMBNAIL_SIZE, height);
    drawImage(thumbnailNode, width, height, pixels, { centered: true });

    thumbnailNode.className = "filmstrip-thumbnail";
    thumbnailNode.onmousedown = e => this._onThumbnailClick(e, index);
    thumbnailNode.setAttribute("index", index);
    this._filmstrip.appendChild(thumbnailNode);
  },

  /**
   * Sets the currently highlighted thumbnail in this container.
   * A screenshot will always correlate to a thumbnail in the filmstrip,
   * both being identified by the same 'index' of the context function call.
   *
   * @param number index
   *        The context function call's index.
   */
  set highlightedThumbnail(index) {
    let currHighlightedThumbnail = $(".filmstrip-thumbnail[index='" + index + "']");
    if (currHighlightedThumbnail == null) {
      return;
    }

    let prevIndex = this._highlightedThumbnailIndex
    let prevHighlightedThumbnail = $(".filmstrip-thumbnail[index='" + prevIndex + "']");
    if (prevHighlightedThumbnail) {
      prevHighlightedThumbnail.removeAttribute("highlighted");
    }

    currHighlightedThumbnail.setAttribute("highlighted", "");
    currHighlightedThumbnail.scrollIntoView();
    this._highlightedThumbnailIndex = index;
  },

  /**
   * Gets the currently highlighted thumbnail in this container.
   * @return number
   */
  get highlightedThumbnail() {
    return this._highlightedThumbnailIndex;
  },

  /**
   * The select listener for this container.
   */
  _onSelect: function({ detail: callItem }) {
    if (!callItem) {
      return;
    }

    // Some of the stepping buttons don't make sense specifically while the
    // last function call is selected.
    if (this.selectedIndex == this.itemCount - 1) {
      $("#resume").setAttribute("disabled", "true");
      $("#step-over").setAttribute("disabled", "true");
      $("#step-out").setAttribute("disabled", "true");
    } else {
      $("#resume").removeAttribute("disabled");
      $("#step-over").removeAttribute("disabled");
      $("#step-out").removeAttribute("disabled");
    }

    // Correlate the currently selected item with the function selection
    // slider's value. Avoid triggering a redundant selection event.
    this._ignoreSliderChanges = true;
    this._slider.value = this.selectedIndex;
    this._ignoreSliderChanges = false;

    // Can't generate screenshots for function call actors loaded from disk.
    // XXX: Bug 984844.
    if (callItem.attachment.actor.isLoadedFromDisk) {
      return;
    }

    // To keep continuous selection buttery smooth (for example, while pressing
    // the DOWN key or moving the slider), only display the screenshot after
    // any kind of user input stops.
    setConditionalTimeout("screenshot-display", SCREENSHOT_DISPLAY_DELAY, () => {
      return !this._isSliding;
    }, () => {
      let frameSnapshot = SnapshotsListView.selectedItem.attachment.actor
      let functionCall = callItem.attachment.actor;
      frameSnapshot.generateScreenshotFor(functionCall).then(screenshot => {
        this.showScreenshot(screenshot);
        this.highlightedThumbnail = screenshot.index;
      }).catch(Cu.reportError);
    });
  },

  /**
   * The mousedown listener for the call selection slider.
   */
  _onSlideMouseDown: function() {
    this._isSliding = true;
  },

  /**
   * The mouseup listener for the call selection slider.
   */
  _onSlideMouseUp: function() {
    this._isSliding = false;
  },

  /**
   * The change listener for the call selection slider.
   */
  _onSlide: function() {
    // Avoid performing any operations when programatically changing the value.
    if (this._ignoreSliderChanges) {
      return;
    }
    let selectedFunctionCallIndex = this.selectedIndex = this._slider.value;

    // While sliding, immediately show the most relevant thumbnail for a
    // function call, for a nice diff-like animation effect between draws.
    let thumbnails = SnapshotsListView.selectedItem.attachment.thumbnails;
    let thumbnail = getThumbnailForCall(thumbnails, selectedFunctionCallIndex);

    // Avoid drawing and highlighting if the selected function call has the
    // same thumbnail as the last one.
    if (thumbnail.index == this.highlightedThumbnail) {
      return;
    }
    // If a thumbnail wasn't found (e.g. the backend avoids creating thumbnails
    // when rendering offscreen), simply defer to the first available one.
    if (thumbnail.index == -1) {
      thumbnail = thumbnails[0];
    }

    let { index, width, height, flipped, pixels } = thumbnail;
    this.highlightedThumbnail = index;

    let screenshotNode = $("#screenshot-image");
    screenshotNode.setAttribute("flipped", flipped);
    drawBackground("screenshot-rendering", width, height, pixels);
  },

  /**
   * The input listener for the calls searchbox.
   */
  _onSearch: function(e) {
    let lowerCaseSearchToken = this._searchbox.value.toLowerCase();

    this.filterContents(e => {
      let call = e.attachment.actor;
      let name = call.name.toLowerCase();
      let file = call.file.toLowerCase();
      let line = call.line.toString().toLowerCase();
      let args = call.argsPreview.toLowerCase();

      return name.contains(lowerCaseSearchToken) ||
             file.contains(lowerCaseSearchToken) ||
             line.contains(lowerCaseSearchToken) ||
             args.contains(lowerCaseSearchToken);
    });
  },

  /**
   * The wheel listener for the filmstrip that contains all the thumbnails.
   */
  _onScroll: function(e) {
    this._filmstrip.scrollLeft += e.deltaX;
  },

  /**
   * The click/dblclick listener for an item or location url in this container.
   * When expanding an item, it's corresponding call stack will be displayed.
   */
  _onExpand: function(e) {
    let callItem = this.getItemForElement(e.target);
    let view = $(".call-item-view", callItem.target);

    // If the call stack nodes were already created, simply re-show them
    // or jump to the corresponding file and line in the Debugger if a
    // location link was clicked.
    if (view.hasAttribute("call-stack-populated")) {
      let isExpanded = view.getAttribute("call-stack-expanded") == "true";

      // If clicking on the location, jump to the Debugger.
      if (e.target.classList.contains("call-item-location")) {
        let { file, line } = callItem.attachment.actor;
        this._viewSourceInDebugger(file, line);
        return;
      }
      // Otherwise hide the call stack.
      else {
        view.setAttribute("call-stack-expanded", !isExpanded);
        $(".call-item-stack", view).hidden = isExpanded;
        return;
      }
    }

    let list = document.createElement("vbox");
    list.className = "call-item-stack";
    view.setAttribute("call-stack-populated", "");
    view.setAttribute("call-stack-expanded", "true");
    view.appendChild(list);

    /**
     * Creates a function call nodes in this container for a stack.
     */
    let display = stack => {
      for (let i = 1; i < stack.length; i++) {
        let call = stack[i];

        let contents = document.createElement("hbox");
        contents.className = "call-item-stack-fn";
        contents.style.MozPaddingStart = (i * STACK_FUNC_INDENTATION) + "px";

        let name = document.createElement("label");
        name.className = "plain call-item-stack-fn-name";
        name.setAttribute("value", "↳ " + call.name + "()");
        contents.appendChild(name);

        let spacer = document.createElement("spacer");
        spacer.setAttribute("flex", "100");
        contents.appendChild(spacer);

        let location = document.createElement("label");
        location.className = "plain call-item-stack-fn-location";
        location.setAttribute("value", getFileName(call.file) + ":" + call.line);
        location.setAttribute("crop", "start");
        location.setAttribute("flex", "1");
        location.addEventListener("mousedown", e => this._onStackFileClick(e, call));
        contents.appendChild(location);

        list.appendChild(contents);
      }

      window.emit(EVENTS.CALL_STACK_DISPLAYED);
    };

    // If this animation snapshot is loaded from disk, there are no corresponding
    // backend actors available and the data is immediately available.
    let functionCall = callItem.attachment.actor;
    if (functionCall.isLoadedFromDisk) {
      display(functionCall.stack);
    }
    // ..otherwise we need to request the function call stack from the backend.
    else {
      callItem.attachment.actor.getDetails().then(fn => display(fn.stack));
    }
  },

  /**
   * The click listener for a location link in the call stack.
   *
   * @param string file
   *        The url of the source owning the function.
   * @param number line
   *        The line of the respective function.
   */
  _onStackFileClick: function(e, { file, line }) {
    this._viewSourceInDebugger(file, line);
  },

  /**
   * The click listener for a thumbnail in the filmstrip.
   *
   * @param number index
   *        The function index in the recorded animation frame snapshot.
   */
  _onThumbnailClick: function(e, index) {
    this.selectedIndex = index;
  },

  /**
   * The click listener for the "resume" button in this container's toolbar.
   */
  _onResume: function() {
    // Jump to the next draw call in the recorded animation frame snapshot.
    let drawCall = getNextDrawCall(this.items, this.selectedItem);
    if (drawCall) {
      this.selectedItem = drawCall;
      return;
    }

    // If there are no more draw calls, just jump to the last context call.
    this._onStepOut();
  },

  /**
   * The click listener for the "step over" button in this container's toolbar.
   */
  _onStepOver: function() {
    this.selectedIndex++;
  },

  /**
   * The click listener for the "step in" button in this container's toolbar.
   */
  _onStepIn: function() {
    if (this.selectedIndex == -1) {
      this._onResume();
      return;
    }
    let callItem = this.selectedItem;
    let { file, line } = callItem.attachment.actor;
    this._viewSourceInDebugger(file, line);
  },

  /**
   * The click listener for the "step out" button in this container's toolbar.
   */
  _onStepOut: function() {
    this.selectedIndex = this.itemCount - 1;
  },

  /**
   * Opens the specified file and line in the debugger. Falls back to Firefox's View Source.
   */
  _viewSourceInDebugger: function (file, line) {
    gToolbox.viewSourceInDebugger(file, line).then(success => {
      if (success) {
        window.emit(EVENTS.SOURCE_SHOWN_IN_JS_DEBUGGER);
      } else {
        window.emit(EVENTS.SOURCE_NOT_FOUND_IN_JS_DEBUGGER);
      }
    });
  }
});
