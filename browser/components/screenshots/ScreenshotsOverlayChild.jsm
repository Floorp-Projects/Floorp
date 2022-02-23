/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The Screenshots overlay is inserted into the document's
 * canvasFrame anonymous content container (see dom/webidl/Document.webidl).
 *
 * This container gets cleared automatically when the document navigates.
 *
 * Since the overlay markup is inserted in the canvasFrame using
 * insertAnonymousContent, this means that it can be modified using the API
 * described in AnonymousContent.webidl.
 *
 * Any mutation of this content must be via the AnonymousContent API.
 * This is similar in design to [devtools' highlighters](https://firefox-source-docs.mozilla.org/devtools/tools/highlighters.html#inserting-content-in-the-page),
 * though as Screenshots doesnt need to work on XUL documents, or allow multiple kinds of
 * highlight/overlay our case is a little simpler.
 *
 * To retrieve the AnonymousContent instance, use the `content` getter.
 */

var EXPORTED_SYMBOLS = ["ScreenshotsOverlayChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "overlayLocalization", () => {
  return new Localization(["browser/screenshotsOverlay.ftl"], true);
});

const STYLESHEET_URL =
  "chrome://browser/content/screenshots/overlay/overlay.css";
const HTML_NS = "http://www.w3.org/1999/xhtml";

class AnonymousContentOverlay {
  constructor(contentDocument, screenshotsChild) {
    this.listeners = new Map();
    this.elements = new Map();

    this.screenshotsChild = screenshotsChild;

    this.contentDocument = contentDocument;
    // aliased for easier diffs/maintenance of the event management code borrowed from devtools highlighters
    this.pageListenerTarget = contentDocument.ownerGlobal;

    this.overlayFragment = null;

    this.overlayId = "screenshots-overlay-container";
    this._initialized = false;
  }
  get content() {
    if (!this._content || Cu.isDeadWrapper(this._content)) {
      return null;
    }
    return this._content;
  }
  async initialize() {
    if (this._initialized) {
      return;
    }

    let document = this.contentDocument;
    let window = document.ownerGlobal;

    // Inject stylesheet
    if (!this.overlayFragment) {
      try {
        window.windowUtils.loadSheetUsingURIString(
          STYLESHEET_URL,
          window.windowUtils.AGENT_SHEET
        );
      } catch {
        // The method fails if the url is already loaded.
      }
    }

    // Inject markup for the overlay UI
    this.overlayFragment = this.overlayFragment
      ? this.overlayFragment
      : this.buildOverlay();

    this._content = document.insertAnonymousContent(
      this.overlayFragment.children[0]
    );

    this.addEventListenerForElement(
      "screenshots-cancel-button",
      "click",
      (event, targetId) => {
        this.screenshotsChild.requestCancelScreenshot();
      }
    );

    // TODO:
    // * Define & hook up drag handles for custom region selection

    this._initialized = true;
  }

  tearDown() {
    if (this._content) {
      this._removeAllListeners();
      try {
        this.contentDocument.removeAnonymousContent(this._content);
      } catch (e) {
        // If the current window isn't the one the content was inserted into, this
        // will fail, but that's fine.
      }
    }
    this._initialized = false;
  }

  createElem(elemName, props = {}, className = "") {
    let elem = this.contentDocument.createElementNS(HTML_NS, elemName);
    if (props) {
      for (let [name, value] of Object.entries(props)) {
        elem[name] = value;
      }
    }
    if (className) {
      elem.className = className;
    }
    // register this element so we can target events at it
    if (elem.id) {
      this.elements.set(elem.id, elem);
    }
    return elem;
  }

  buildOverlay() {
    let [cancel, instrustions] = overlayLocalization.formatMessagesSync([
      { id: "screenshots-overlay-cancel-button" },
      { id: "screenshots-overlay-instructions" },
    ]);

    const htmlString = `
    <div id="${this.overlayId}">
      <div class="fixed-container">
       <div class="face-container">
         <div class="eye left"><div class="eyeball"></div></div>
         <div class="eye right"><div class="eyeball"></div></div>
         <div class="face"></div>
       </div>
       <div class="preview-instructions" data-l10n-id="screenshots-instructions">${instrustions.value}</div>
       <div class="cancel-shot" id="screenshots-cancel-button" data-l10n-id="screenshots-overlay-cancel-button">${cancel.value}</div>
      </div>
    </div`;

    const parser = new this.contentDocument.ownerGlobal.DOMParser();
    const tmpDoc = parser.parseFromString(htmlString, "text/html");
    const fragment = this.contentDocument.createDocumentFragment();

    fragment.appendChild(tmpDoc.body.children[0]);
    return fragment;
  }

  // The event tooling is borrowed directly from devtools' highlighters (CanvasFrameAnonymousContentHelper)
  /**
   * Add an event listener to one of the elements inserted in the canvasFrame
   * native anonymous container.
   * Like other methods in this helper, this requires the ID of the element to
   * be passed in.
   *
   * Note that if the content page navigates, the event listeners won't be
   * added again.
   *
   * Also note that unlike traditional DOM events, the events handled by
   * listeners added here will propagate through the document only through
   * bubbling phase, so the useCapture parameter isn't supported.
   * It is possible however to call e.stopPropagation() to stop the bubbling.
   *
   * IMPORTANT: the chrome-only canvasFrame insertion API takes great care of
   * not leaking references to inserted elements to chrome JS code. That's
   * because otherwise, chrome JS code could freely modify native anon elements
   * inside the canvasFrame and probably change things that are assumed not to
   * change by the C++ code managing this frame.
   * See https://wiki.mozilla.org/DevTools/Highlighter#The_AnonymousContent_API
   * Unfortunately, the inserted nodes are still available via
   * event.originalTarget, and that's what the event handler here uses to check
   * that the event actually occured on the right element, but that also means
   * consumers of this code would be able to access the inserted elements.
   * Therefore, the originalTarget property will be nullified before the event
   * is passed to your handler.
   *
   * IMPL DETAIL: A single event listener is added per event types only, at
   * browser level and if the event originalTarget is found to have the provided
   * ID, the callback is executed (and then IDs of parent nodes of the
   * originalTarget are checked too).
   *
   * @param {String} id
   * @param {String} type
   * @param {Function} handler
   */
  addEventListenerForElement(id, type, handler) {
    if (typeof id !== "string") {
      throw new Error(
        "Expected a string ID in addEventListenerForElement but got: " + id
      );
    }

    // If no one is listening for this type of event yet, add one listener.
    if (!this.listeners.has(type)) {
      const target = this.pageListenerTarget;
      target.addEventListener(type, this, true);
      // Each type entry in the map is a map of ids:handlers.
      this.listeners.set(type, new Map());
    }

    const listeners = this.listeners.get(type);
    listeners.set(id, handler);
  }

  /**
   * Remove an event listener from one of the elements inserted in the
   * canvasFrame native anonymous container.
   * @param {String} id
   * @param {String} type
   */
  removeEventListenerForElement(id, type) {
    const listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(id);

    // If no one is listening for event type anymore, remove the listener.
    if (!listeners.size) {
      const target = this.pageListenerTarget;
      target.removeEventListener(type, this, true);
    }
  }

  handleEvent(event) {
    const listeners = this.listeners.get(event.type);
    if (!listeners) {
      return;
    }

    // Hide the originalTarget property to avoid exposing references to native
    // anonymous elements. See addEventListenerForElement's comment.
    let isPropagationStopped = false;
    const eventProxy = new Proxy(event, {
      get: (obj, name) => {
        if (name === "originalTarget") {
          return null;
        } else if (name === "stopPropagation") {
          return () => {
            isPropagationStopped = true;
          };
        }
        return obj[name];
      },
    });

    // Start at originalTarget, bubble through ancestors and call handlers when
    // needed.
    let node = event.originalTarget;
    while (node) {
      let nodeId = node.id;
      if (nodeId) {
        const handler = listeners.get(node.id);
        if (handler) {
          handler(eventProxy, nodeId);
          if (isPropagationStopped) {
            break;
          }
        }
        if (nodeId == this.overlayId) {
          break;
        }
      }
      node = node.parentNode;
    }
  }

  _removeAllListeners() {
    if (this.pageListenerTarget) {
      const target = this.pageListenerTarget;
      for (const [type] of this.listeners) {
        target.removeEventListener(type, this, true);
      }
    }
    this.listeners.clear();
  }
}

var ScreenshotsOverlayChild = {
  AnonymousContentOverlay,
};
