/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from ../debugger-controller.js */
/* import-globals-from ../debugger-view.js */
/* import-globals-from ../utils.js */
/* globals document */
"use strict";

/*
 * Functions handling the stackframes classic list UI.
 * Controlled by the DebuggerView.StackFrames isntance.
 */
function StackFramesClassicListView(DebuggerController, DebuggerView) {
  dumpn("StackFramesClassicListView was instantiated");

  this.DebuggerView = DebuggerView;
  this._onSelect = this._onSelect.bind(this);
}

StackFramesClassicListView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function () {
    dumpn("Initializing the StackFramesClassicListView");

    this.widget = new SideMenuWidget(document.getElementById("callstack-list"));
    this.widget.addEventListener("select", this._onSelect, false);

    this.emptyText = L10N.getStr("noStackFramesText");
    this.autoFocusOnFirstItem = false;
    this.autoFocusOnSelection = false;

    // This view's contents are also mirrored in a different container.
    this._mirror = this.DebuggerView.StackFrames;
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function () {
    dumpn("Destroying the StackFramesClassicListView");

    this.widget.removeEventListener("select", this._onSelect, false);
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
   */
  addFrame: function (aTitle, aUrl, aLine, aDepth) {
    // Create the element node for the stack frame item.
    let frameView = this._createFrameView.apply(this, arguments);

    // Append a stack frame item to this container.
    this.push([frameView], {
      attachment: {
        depth: aDepth
      }
    });
  },

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
   * @return nsIDOMNode
   *         The stack frame view.
   */
  _createFrameView: function (aTitle, aUrl, aLine, aDepth) {
    let container = document.createElement("hbox");
    container.id = "classic-stackframe-" + aDepth;
    container.className = "dbg-classic-stackframe";
    container.setAttribute("flex", "1");

    let frameTitleNode = document.createElement("label");
    frameTitleNode.className = "plain dbg-classic-stackframe-title";
    frameTitleNode.setAttribute("value", aTitle);
    frameTitleNode.setAttribute("crop", "center");

    let frameDetailsNode = document.createElement("hbox");
    frameDetailsNode.className = "plain dbg-classic-stackframe-details";

    let frameUrlNode = document.createElement("label");
    frameUrlNode.className = "plain dbg-classic-stackframe-details-url";
    frameUrlNode.setAttribute("value", SourceUtils.getSourceLabel(aUrl));
    frameUrlNode.setAttribute("crop", "center");
    frameDetailsNode.appendChild(frameUrlNode);

    let frameDetailsSeparator = document.createElement("label");
    frameDetailsSeparator.className = "plain dbg-classic-stackframe-details-sep";
    frameDetailsSeparator.setAttribute("value", SEARCH_LINE_FLAG);
    frameDetailsNode.appendChild(frameDetailsSeparator);

    let frameLineNode = document.createElement("label");
    frameLineNode.className = "plain dbg-classic-stackframe-details-line";
    frameLineNode.setAttribute("value", aLine);
    frameDetailsNode.appendChild(frameLineNode);

    container.appendChild(frameTitleNode);
    container.appendChild(frameDetailsNode);

    return container;
  },

  /**
   * The select listener for the stackframes container.
   */
  _onSelect: function (e) {
    let stackframeItem = this.selectedItem;
    if (stackframeItem) {
      // The container is not empty and an actual item was selected.
      // Mirror the selected item in the breadcrumbs list.
      let depth = stackframeItem.attachment.depth;
      this._mirror.selectedItem = e => e.attachment.depth == depth;
    }
  },

  _mirror: null
});

DebuggerView.StackFramesClassicList = new StackFramesClassicListView(DebuggerController,
                                                                     DebuggerView);
