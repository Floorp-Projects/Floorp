/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/inspector.properties");

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

    const sourceMapService = this._toolbox.sourceMapURLService;

    const Bubbling = L10N.getStr("eventsTooltip.Bubbling");
    const Capturing = L10N.getStr("eventsTooltip.Capturing");
    for (const listener of this._eventListenerInfos) {
      const phase = listener.capturing ? Capturing : Bubbling;
      const level = listener.DOM0 ? "DOM0" : "DOM2";

      // Create this early so we can refer to it from a closure, below.
      const content = doc.createElementNS(XHTML_NS, "div");

      // Header
      const header = doc.createElementNS(XHTML_NS, "div");
      header.className = "event-header devtools-toolbar";
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

      let text = listener.origin;
      let title = text;
      if (listener.hide.filename) {
        text = L10N.getStr("eventsTooltip.unknownLocation");
        title = L10N.getStr("eventsTooltip.unknownLocationExplanation");
      } else {
        const location = this._parseLocation(text);
        if (location) {
          const callback = (enabled, url, line, column) => {
            // Do nothing if the tooltip was destroyed while we were
            // waiting for a response.
            if (this._tooltip) {
              const newUrl = enabled ? url : location.url;
              const newLine = enabled ? line : location.line;

              const newURI = newUrl + ":" + newLine;
              filename.textContent = newURI;
              filename.setAttribute("title", newURI);
              const eventEditor = this._eventEditors.get(content);
              eventEditor.uri = newURI;

              // Since we just changed the URI/line, we need to clear out the
              // source actor ID. These must be consistent with each other when
              // we call viewSourceInDebugger with the event's information.
              eventEditor.sourceActor = null;

              // This is emitted for testing.
              this._tooltip.emit("event-tooltip-source-map-ready");
            }
          };

          sourceMapService.subscribe(location.url, location.line, null, callback);
          this._subscriptions.push({
            url: location.url,
            line: location.line,
            callback,
          });
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
        uri: listener.origin,
        sourceActor: listener.sourceActor,
        dom0: listener.DOM0,
        native: listener.native,
        appended: false,
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
    header.classList.toggle("content-expanded");

    if (content.hasAttribute("open")) {
      content.removeAttribute("open");
    } else {
      const contentNodes = doc.querySelectorAll(".event-tooltip-content-box");

      for (const node of contentNodes) {
        if (node !== content) {
          node.removeAttribute("open");
        }
      }

      content.setAttribute("open", "");

      const eventEditor = this._eventEditors.get(content);

      if (eventEditor.appended) {
        return;
      }

      const {editor, handler} = eventEditor;

      const iframe = doc.createElementNS(XHTML_NS, "iframe");
      iframe.setAttribute("style", "width: 100%; height: 100%; border-style: none;");

      editor.appendTo(content, iframe).then(() => {
        const tidied = beautify.js(handler, { "indent_size": 2 });
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

    const {sourceActor, uri} = this._eventEditors.get(content);

    const location = this._parseLocation(uri);
    if (location) {
      // Save a copy of toolbox as it will be set to null when we hide the tooltip.
      const toolbox = this._toolbox;

      this._tooltip.hide();

      toolbox.viewSourceInDebugger(location.url, location.line,
        location.column, sourceActor);
    }
  },

  /**
   * Parse URI and return {url, line}; or return null if it can't be parsed.
   */
  _parseLocation: function(uri) {
    if (uri && uri !== "?") {
      uri = uri.replace(/"/g, "");

      const matches = uri.match(/(.*):(\d+$)/);

      if (matches) {
        return {
          url: matches[1],
          line: parseInt(matches[2], 10),
        };
      }
      return {url: uri, line: 1};
    }
    return null;
  },

  destroy: function() {
    if (this._tooltip) {
      this._tooltip.off("hidden", this.destroy);

      const boxes = this.container.querySelectorAll(".event-tooltip-content-box");

      for (const box of boxes) {
        const {editor} = this._eventEditors.get(box);
        editor.destroy();
      }

      this._eventEditors = null;
      this._tooltip.eventTooltip = null;
    }

    const headerNodes = this.container.querySelectorAll(".event-header");

    for (const node of headerNodes) {
      node.removeEventListener("click", this._headerClicked);
    }

    const sourceNodes = this.container.querySelectorAll(".event-tooltip-debugger-icon");
    for (const node of sourceNodes) {
      node.removeEventListener("click", this._debugClicked);
    }

    const sourceMapService = this._toolbox.sourceMapURLService;
    for (const subscription of this._subscriptions) {
      sourceMapService.unsubscribe(subscription.url, subscription.line, null,
                                   subscription.callback);
    }

    this._eventListenerInfos = this._toolbox = this._tooltip = null;
  },
};

module.exports.setEventTooltip = setEventTooltip;
