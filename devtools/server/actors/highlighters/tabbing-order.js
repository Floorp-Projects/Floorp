/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "ContentDOMReference",
  "resource://gre/modules/ContentDOMReference.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  ["isFrameWithChildTarget", "isWindowIncluded"],
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "NodeTabbingOrderHighlighter",
  "devtools/server/actors/highlighters/node-tabbing-order",
  true
);

const DEFAULT_FOCUS_FLAGS = Services.focus.FLAG_NOSCROLL;

/**
 * The TabbingOrderHighlighter uses focus manager to traverse all focusable
 * nodes on the page and then uses the NodeTabbingOrderHighlighter to highlight
 * these nodes.
 */
class TabbingOrderHighlighter {
  constructor(highlighterEnv) {
    this.highlighterEnv = highlighterEnv;
    this._highlighters = new Map();

    this.onMutation = this.onMutation.bind(this);
    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);
  }

  /**
   * Static getter that indicates that TabbingOrderHighlighter supports
   * highlighting in XUL windows.
   */
  static get XULSupported() {
    return true;
  }

  get win() {
    return this.highlighterEnv.window;
  }

  get focusedElement() {
    return Services.focus.getFocusedElementForWindow(this.win, true, {});
  }

  set focusedElement(element) {
    Services.focus.setFocus(element, DEFAULT_FOCUS_FLAGS);
  }

  moveFocus(startElement) {
    return Services.focus.moveFocus(
      this.win,
      startElement.nodeType === Node.DOCUMENT_NODE
        ? startElement.documentElement
        : startElement,
      Services.focus.MOVEFOCUS_FORWARD,
      DEFAULT_FOCUS_FLAGS
    );
  }

  /**
   * Show NodeTabbingOrderHighlighter on each node that belongs to the keyboard
   * tabbing order.
   *
   * @param  {DOMNode} startElm
   *         Starting element to calculate tabbing order from.
   *
   * @param  {JSON} options
   *         - options.index
   *           Start index for the tabbing order. Starting index will be 0 at
   *           the start of the tabbing order highlighting; in remote frames
   *           starting index will, typically, be greater than 0 (unless there
   *           was nothing to focus in the top level content document prior to
   *           the remote frame).
   */
  async show(startElm, { index }) {
    const focusableElements = [];
    const originalFocusedElement = this.focusedElement;
    let currentFocusedElement = this.moveFocus(startElm);
    while (
      currentFocusedElement &&
      isWindowIncluded(this.win, currentFocusedElement.ownerGlobal)
    ) {
      focusableElements.push(currentFocusedElement);
      currentFocusedElement = this.moveFocus(currentFocusedElement);
    }

    // Allow to flush pending notifications to ensure the PresShell and frames
    // are updated.
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
    let endElm = this.focusedElement;
    if (
      currentFocusedElement &&
      !isWindowIncluded(this.win, currentFocusedElement.ownerGlobal)
    ) {
      endElm = null;
    }

    if (
      !endElm &&
      !!focusableElements.length &&
      isFrameWithChildTarget(
        this.highlighterEnv.targetActor,
        focusableElements[focusableElements.length - 1]
      )
    ) {
      endElm = focusableElements[focusableElements.length - 1];
    }

    if (originalFocusedElement && originalFocusedElement !== endElm) {
      this.focusedElement = originalFocusedElement;
    }

    const highlighters = [];
    for (let i = 0; i < focusableElements.length; i++) {
      highlighters.push(
        this._accumulateHighlighter(focusableElements[i], index++)
      );
    }
    await Promise.all(highlighters);

    this._trackMutations();

    return {
      contentDOMReference: endElm && ContentDOMReference.get(endElm),
      index,
    };
  }

  async _accumulateHighlighter(node, index) {
    const highlighter = new NodeTabbingOrderHighlighter(this.highlighterEnv);
    await highlighter.isReady;

    highlighter.show(node, { index: index + 1 });
    this._highlighters.set(node, highlighter);
  }

  hide() {
    this._untrackMutations();
    for (const highlighter of this._highlighters.values()) {
      highlighter.destroy();
    }

    this._highlighters.clear();
  }

  /**
   * Track mutations in the top level document subtree so that the appropriate
   * NodeTabbingOrderHighlighter infobar's could be updated to reflect the
   * attribute mutations on relevant nodes.
   */
  _trackMutations() {
    const { win } = this;
    this.currentMutationObserver = new win.MutationObserver(this.onMutation);
    this.currentMutationObserver.observe(win.document.documentElement, {
      subtree: true,
      attributes: true,
    });
  }

  _untrackMutations() {
    if (!this.currentMutationObserver) {
      return;
    }

    this.currentMutationObserver.disconnect();
    this.currentMutationObserver = null;
  }

  onMutation(mutationList) {
    for (const { target } of mutationList) {
      const highlighter = this._highlighters.get(target);
      if (highlighter) {
        highlighter.update();
      }
    }
  }

  /**
   * Update NodeTabbingOrderHighlighter focus styling for a node that,
   * potentially, belongs to the tabbing order.
   * @param {Object} options
   *        Options specifying the node and its focused state.
   */
  updateFocus({ node, focused }) {
    const highlighter = this._highlighters.get(node);
    if (!highlighter) {
      return;
    }

    highlighter.updateFocus(focused);
  }

  destroy() {
    this.highlighterEnv.off("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = this.highlighterEnv;
    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    this.hide();
    this.highlighterEnv = null;
  }

  onPageHide({ target }) {
    // If a pagehide event is triggered for current window's highlighter, hide
    // the highlighter.
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  onWillNavigate({ isTopLevel }) {
    if (isTopLevel) {
      this.hide();
    }
  }
}

exports.TabbingOrderHighlighter = TabbingOrderHighlighter;
