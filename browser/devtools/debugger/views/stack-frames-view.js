/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Functions handling the stackframes UI.
 */
function StackFramesView(DebuggerController, DebuggerView) {
  dumpn("StackFramesView was instantiated");

  this.StackFrames = DebuggerController.StackFrames;
  this.DebuggerView = DebuggerView;

  this._onStackframeRemoved = this._onStackframeRemoved.bind(this);
  this._onSelect = this._onSelect.bind(this);
  this._onScroll = this._onScroll.bind(this);
  this._afterScroll = this._afterScroll.bind(this);
}

StackFramesView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the StackFramesView");

    this.widget = new BreadcrumbsWidget(document.getElementById("stackframes"));
    this.widget.addEventListener("select", this._onSelect, false);
    this.widget.addEventListener("scroll", this._onScroll, true);
    window.addEventListener("resize", this._onScroll, true);

    this.autoFocusOnFirstItem = false;
    this.autoFocusOnSelection = false;

    // This view's contents are also mirrored in a different container.
    this._mirror = this.DebuggerView.StackFramesClassicList;
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the StackFramesView");

    this.widget.removeEventListener("select", this._onSelect, false);
    this.widget.removeEventListener("scroll", this._onScroll, true);
    window.removeEventListener("resize", this._onScroll, true);
  },

  /**
   * Adds a frame in this stackframes container.
   *
   * @param string aTitle
   *        The frame title (function name).
   * @param string aUrl
   *        The frame source url.
   * @param string aLine
   *        The frame line number.
   * @param number aDepth
   *        The frame depth in the stack.
   * @param boolean aIsBlackBoxed
   *        Whether or not the frame is black boxed.
   */
  addFrame: function(aTitle, aUrl, aLine, aDepth, aIsBlackBoxed) {
    // Blackboxed stack frames are collapsed into a single entry in
    // the view. By convention, only the first frame is displayed.
    if (aIsBlackBoxed) {
      if (this._prevBlackBoxedUrl == aUrl) {
        return;
      }
      this._prevBlackBoxedUrl = aUrl;
    } else {
      this._prevBlackBoxedUrl = null;
    }

    // Create the element node for the stack frame item.
    let frameView = this._createFrameView.apply(this, arguments);

    // Append a stack frame item to this container.
    this.push([frameView], {
      index: 0, /* specifies on which position should the item be appended */
      attachment: {
        title: aTitle,
        url: aUrl,
        line: aLine,
        depth: aDepth
      },
      // Make sure that when the stack frame item is removed, the corresponding
      // mirrored item in the classic list is also removed.
      finalize: this._onStackframeRemoved
    });

    // Mirror this newly inserted item inside the "Call Stack" tab.
    this._mirror.addFrame(aTitle, aUrl, aLine, aDepth);
  },

  /**
   * Selects the frame at the specified depth in this container.
   * @param number aDepth
   */
  set selectedDepth(aDepth) {
    this.selectedItem = aItem => aItem.attachment.depth == aDepth;
  },

  /**
   * Gets the currently selected stack frame's depth in this container.
   * This will essentially be the opposite of |selectedIndex|, which deals
   * with the position in the view, where the last item added is actually
   * the bottommost, not topmost.
   * @return number
   */
  get selectedDepth() {
    return this.selectedItem.attachment.depth;
  },

  /**
   * Specifies if the active thread has more frames that need to be loaded.
   */
  dirty: false,

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aTitle
   *        The frame title to be displayed in the list.
   * @param string aUrl
   *        The frame source url.
   * @param string aLine
   *        The frame line number.
   * @param number aDepth
   *        The frame depth in the stack.
   * @param boolean aIsBlackBoxed
   *        Whether or not the frame is black boxed.
   * @return nsIDOMNode
   *         The stack frame view.
   */
  _createFrameView: function(aTitle, aUrl, aLine, aDepth, aIsBlackBoxed) {
    let container = document.createElement("hbox");
    container.id = "stackframe-" + aDepth;
    container.className = "dbg-stackframe";

    let frameDetails = SourceUtils.trimUrlLength(
      SourceUtils.getSourceLabel(aUrl),
      STACK_FRAMES_SOURCE_URL_MAX_LENGTH,
      STACK_FRAMES_SOURCE_URL_TRIM_SECTION);

    if (aIsBlackBoxed) {
      container.classList.add("dbg-stackframe-black-boxed");
    } else {
      let frameTitleNode = document.createElement("label");
      frameTitleNode.className = "plain dbg-stackframe-title breadcrumbs-widget-item-tag";
      frameTitleNode.setAttribute("value", aTitle);
      container.appendChild(frameTitleNode);

      frameDetails += SEARCH_LINE_FLAG + aLine;
    }

    let frameDetailsNode = document.createElement("label");
    frameDetailsNode.className = "plain dbg-stackframe-details breadcrumbs-widget-item-id";
    frameDetailsNode.setAttribute("value", frameDetails);
    container.appendChild(frameDetailsNode);

    return container;
  },

  /**
   * Function called each time a stack frame item is removed.
   *
   * @param object aItem
   *        The corresponding item.
   */
  _onStackframeRemoved: function(aItem) {
    dumpn("Finalizing stackframe item: " + aItem.stringify());

    // Remove the mirrored item in the classic list.
    let depth = aItem.attachment.depth;
    this._mirror.remove(this._mirror.getItemForAttachment(e => e.depth == depth));

    // Forget the previously blackboxed stack frame url.
    this._prevBlackBoxedUrl = null;
  },

  /**
   * The select listener for the stackframes container.
   */
  _onSelect: function(e) {
    let stackframeItem = this.selectedItem;
    if (stackframeItem) {
      // The container is not empty and an actual item was selected.
      let depth = stackframeItem.attachment.depth;

      // Mirror the selected item in the classic list.
      this.suppressSelectionEvents = true;
      this._mirror.selectedItem = e => e.attachment.depth == depth;
      this.suppressSelectionEvents = false;

      DebuggerController.StackFrames.selectFrame(depth);
    }
  },

  /**
   * The scroll listener for the stackframes container.
   */
  _onScroll: function() {
    // Update the stackframes container only if we have to.
    if (!this.dirty) {
      return;
    }
    // Allow requests to settle down first.
    setNamedTimeout("stack-scroll", STACK_FRAMES_SCROLL_DELAY, this._afterScroll);
  },

  /**
   * Requests the addition of more frames from the controller.
   */
  _afterScroll: function() {
    let scrollPosition = this.widget.getAttribute("scrollPosition");
    let scrollWidth = this.widget.getAttribute("scrollWidth");

    // If the stackframes container scrolled almost to the end, with only
    // 1/10 of a breadcrumb remaining, load more content.
    if (scrollPosition - scrollWidth / 10 < 1) {
      this.ensureIndexIsVisible(CALL_STACK_PAGE_SIZE - 1);
      this.dirty = false;

      // Loads more stack frames from the debugger server cache.
      DebuggerController.StackFrames.addMoreFrames();
    }
  },

  _mirror: null,
  _prevBlackBoxedUrl: null
});

DebuggerView.StackFrames = new StackFramesView(DebuggerController, DebuggerView);
