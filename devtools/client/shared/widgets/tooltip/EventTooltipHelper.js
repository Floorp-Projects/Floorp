/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

const Editor = require("devtools/client/shared/sourceeditor/editor");
const beautify = require("devtools/shared/jsbeautify/beautify");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const CONTAINER_WIDTH = 500;

/**
 * Set the content of a provided HTMLTooltip instance to display a list of event
 * listeners, with their event type, capturing argument and a link to the code
 * of the event handler.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance on which the event details content should be set
 * @param {Array} eventListenerInfos
 *        A list of event listeners
 * @param {Toolbox} toolbox
 *        Toolbox used to select debugger panel
 */
function setEventTooltip(tooltip, eventListenerInfos, toolbox) {
  const eventTooltip = new EventTooltip(tooltip, eventListenerInfos, toolbox);
  eventTooltip.init();
}

function EventTooltip(tooltip, eventListenerInfos, toolbox) {
  this._tooltip = tooltip;
  this._eventListenerInfos = eventListenerInfos;
  this._toolbox = toolbox;
  this._eventEditors = new WeakMap();

  // Used in tests: add a reference to the EventTooltip instance on the HTMLTooltip.
  this._tooltip.eventTooltip = this;

  this._headerClicked = this._headerClicked.bind(this);
  this._debugClicked = this._debugClicked.bind(this);
  this.destroy = this.destroy.bind(this);
  this._subscriptions = [];
}

EventTooltip.prototype = {
  init: function() {
    const config = {
      mode: Editor.modes.js,
      lineNumbers: false,
      lineWrapping: true,
      readOnly: true,
      styleActiveLine: true,
      extraKeys: {},
      theme: "mozilla markup-view",
    };

    const doc = this._tooltip.doc;
    this.container = doc.createElementNS(XHTML_NS, "div");
    this.container.className = "devtools-tooltip-events-container";

    const sourceMapURLService = this._toolbox.sourceMapURLService;

    const Bubbling = L10N.getStr("eventsTooltip.Bubbling");
    const Capturing = L10N.getStr("eventsTooltip.Capturing");
    for (const listener of this._eventListenerInfos) {
      const phase = listener.capturing ? Capturing : Bubbling;
      const level = listener.DOM0 ? "DOM0" : "DOM2";

      // Create this early so we can refer to it from a closure, below.
      const content = doc.createElementNS(XHTML_NS, "div");

      // Header
      const header = doc.createElementNS(XHTML_NS, "div");
      header.className = "event-header";
      const arrow = doc.createElementNS(XHTML_NS, "span");
      arrow.className = "theme-twisty";
      header.appendChild(arrow);
      this.container.appendChild(header);

      if (!listener.hide.type) {
        const eventTypeLabel = doc.createElementNS(XHTML_NS, "span");
        eventTypeLabel.className = "event-tooltip-event-type";
        eventTypeLabel.textContent = listener.type;
        eventTypeLabel.setAttribute("title", listener.type);
        header.appendChild(eventTypeLabel);
      }

      const filename = doc.createElementNS(XHTML_NS, "span");
      filename.className = "event-tooltip-filename devtools-monospace";

      let location = null;
      let text = listener.origin;
      let title = text;
      if (listener.hide.filename) {
        text = L10N.getStr("eventsTooltip.unknownLocation");
        title = L10N.getStr("eventsTooltip.unknownLocationExplanation");
      } else {
        location = this._parseLocation(listener.origin);

        // There will be no source actor if the listener is a native function
        // or wasn't a debuggee, in which case there's also not going to be
        // a sourcemap, so we don't need to worry about subscribing.
        if (location && listener.sourceActor) {
          location.id = listener.sourceActor;

          this._subscriptions.push(
            sourceMapURLService.subscribeByID(
              location.id,
              location.line,
              location.column,
              originalLocation => {
                const currentLoc = originalLocation || location;

                const newURI = currentLoc.url + ":" + currentLoc.line;
                filename.textContent = newURI;
                filename.setAttribute("title", newURI);

                // This is emitted for testing.
                this._tooltip.emit("event-tooltip-source-map-ready");
              }
            )
          );
        }
      }

      filename.textContent = text;
      filename.setAttribute("title", title);
      header.appendChild(filename);

      if (!listener.hide.debugger) {
        const debuggerIcon = doc.createElementNS(XHTML_NS, "div");
        debuggerIcon.className = "event-tooltip-debugger-icon";
        const openInDebugger = L10N.getStr("eventsTooltip.openInDebugger");
        debuggerIcon.setAttribute("title", openInDebugger);
        header.appendChild(debuggerIcon);
      }

      const attributesContainer = doc.createElementNS(XHTML_NS, "div");
      attributesContainer.className = "event-tooltip-attributes-container";
      header.appendChild(attributesContainer);

      if (!listener.hide.capturing) {
        const attributesBox = doc.createElementNS(XHTML_NS, "div");
        attributesBox.className = "event-tooltip-attributes-box";
        attributesContainer.appendChild(attributesBox);

        const capturing = doc.createElementNS(XHTML_NS, "span");
        capturing.className = "event-tooltip-attributes";
        capturing.textContent = phase;
        capturing.setAttribute("title", phase);
        attributesBox.appendChild(capturing);
      }

      if (listener.tags) {
        for (const tag of listener.tags.split(",")) {
          const attributesBox = doc.createElementNS(XHTML_NS, "div");
          attributesBox.className = "event-tooltip-attributes-box";
          attributesContainer.appendChild(attributesBox);

          const tagBox = doc.createElementNS(XHTML_NS, "span");
          tagBox.className = "event-tooltip-attributes";
          tagBox.textContent = tag;
          tagBox.setAttribute("title", tag);
          attributesBox.appendChild(tagBox);
        }
      }

      if (!listener.hide.dom0) {
        const attributesBox = doc.createElementNS(XHTML_NS, "div");
        attributesBox.className = "event-tooltip-attributes-box";
        attributesContainer.appendChild(attributesBox);

        const dom0 = doc.createElementNS(XHTML_NS, "span");
        dom0.className = "event-tooltip-attributes";
        dom0.textContent = level;
        attributesBox.appendChild(dom0);
      }

      // Content
      const editor = new Editor(config);
      this._eventEditors.set(content, {
        editor: editor,
        handler: listener.handler,
        dom0: listener.DOM0,
        native: listener.native,
        appended: false,
        location,
      });

      content.className = "event-tooltip-content-box";
      this.container.appendChild(content);

      this._addContentListeners(header);
    }

    this._tooltip.panel.innerHTML = "";
    this._tooltip.panel.appendChild(this.container);
    this._tooltip.setContentSize({ width: CONTAINER_WIDTH, height: Infinity });
    this._tooltip.on("hidden", this.destroy);
  },

  _addContentListeners: function(header) {
    header.addEventListener("click", this._headerClicked);
  },

  _headerClicked: function(event) {
    if (event.target.classList.contains("event-tooltip-debugger-icon")) {
      this._debugClicked(event);
      event.stopPropagation();
      return;
    }

    const doc = this._tooltip.doc;
    const header = event.currentTarget;
    const content = header.nextElementSibling;

    if (content.hasAttribute("open")) {
      header.classList.remove("content-expanded");
      content.removeAttribute("open");
    } else {
      // Close other open events first
      const openHeaders = doc.querySelectorAll(
        ".event-header.content-expanded"
      );
      const openContent = doc.querySelectorAll(
        ".event-tooltip-content-box[open]"
      );
      for (const node of openHeaders) {
        node.classList.remove("content-expanded");
      }
      for (const node of openContent) {
        node.removeAttribute("open");
      }

      header.classList.add("content-expanded");
      content.setAttribute("open", "");

      const eventEditor = this._eventEditors.get(content);

      if (eventEditor.appended) {
        return;
      }

      const { editor, handler } = eventEditor;

      const iframe = doc.createElementNS(XHTML_NS, "iframe");
      iframe.classList.add("event-tooltip-editor-frame");

      editor.appendTo(content, iframe).then(() => {
        const tidied = beautify.js(handler, { indent_size: 2 });
        editor.setText(tidied);

        eventEditor.appended = true;

        const container = header.parentElement.getBoundingClientRect();
        if (header.getBoundingClientRect().top < container.top) {
          header.scrollIntoView(true);
        } else if (content.getBoundingClientRect().bottom > container.bottom) {
          content.scrollIntoView(false);
        }

        this._tooltip.emit("event-tooltip-ready");
      });
    }
  },

  _debugClicked: function(event) {
    const header = event.currentTarget;
    const content = header.nextElementSibling;

    const { location } = this._eventEditors.get(content);
    if (location) {
      // Save a copy of toolbox as it will be set to null when we hide the tooltip.
      const toolbox = this._toolbox;

      this._tooltip.hide();

      toolbox.viewSourceInDebugger(
        location.url,
        location.line,
        location.column,
        location.id
      );
    }
  },

  /**
   * Parse URI and return {url, line, column}; or return null if it can't be parsed.
   */
  _parseLocation: function(uri) {
    if (uri && uri !== "?") {
      uri = uri.replace(/"/g, "");

      let matches = uri.match(/(.*):(\d+):(\d+$)/);

      if (matches) {
        return {
          url: matches[1],
          line: parseInt(matches[2], 10),
          column: parseInt(matches[3], 10),
        };
      } else if ((matches = uri.match(/(.*):(\d+$)/))) {
        return {
          url: matches[1],
          line: parseInt(matches[2], 10),
          column: null,
        };
      }
      return { url: uri, line: 1, column: null };
    }
    return null;
  },

  destroy: function() {
    if (this._tooltip) {
      this._tooltip.off("hidden", this.destroy);

      const boxes = this.container.querySelectorAll(
        ".event-tooltip-content-box"
      );

      for (const box of boxes) {
        const { editor } = this._eventEditors.get(box);
        editor.destroy();
      }

      this._eventEditors = null;
      this._tooltip.eventTooltip = null;
    }

    const headerNodes = this.container.querySelectorAll(".event-header");

    for (const node of headerNodes) {
      node.removeEventListener("click", this._headerClicked);
    }

    const sourceNodes = this.container.querySelectorAll(
      ".event-tooltip-debugger-icon"
    );
    for (const node of sourceNodes) {
      node.removeEventListener("click", this._debugClicked);
    }

    for (const unsubscribe of this._subscriptions) {
      unsubscribe();
    }

    this._eventListenerInfos = this._toolbox = this._tooltip = null;
  },
};

module.exports.setEventTooltip = setEventTooltip;
