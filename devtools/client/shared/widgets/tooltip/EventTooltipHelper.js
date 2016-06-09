/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
loader.lazyGetter(this, "GetStringFromName", () => {
  let bundle = Services.strings.createBundle(
    "chrome://devtools/locale/inspector.properties");
  return key => {
    return bundle.GetStringFromName(key);
  };
});

loader.lazyRequireGetter(this, "Editor", "devtools/client/sourceeditor/editor");
loader.lazyRequireGetter(this, "beautify", "devtools/shared/jsbeautify/beautify");

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
  let eventTooltip = new EventTooltip(tooltip, eventListenerInfos, toolbox);
  eventTooltip.init();
}

function EventTooltip(tooltip, eventListenerInfos, toolbox) {
  this._tooltip = tooltip;
  this._eventListenerInfos = eventListenerInfos;
  this._toolbox = toolbox;
  this._tooltip.eventEditors = new WeakMap();

  this._headerClicked = this._headerClicked.bind(this);
  this._debugClicked = this._debugClicked.bind(this);
  this.destroy = this.destroy.bind(this);
}

EventTooltip.prototype = {
  init: function () {
    let config = {
      mode: Editor.modes.js,
      lineNumbers: false,
      lineWrapping: true,
      readOnly: true,
      styleActiveLine: true,
      extraKeys: {},
      theme: "mozilla markup-view"
    };

    let doc = this._tooltip.doc;
    this.container = doc.createElementNS(XHTML_NS, "div");
    this.container.className = "devtools-tooltip-events-container";

    for (let listener of this._eventListenerInfos) {
      let phase = listener.capturing ? "Capturing" : "Bubbling";
      let level = listener.DOM0 ? "DOM0" : "DOM2";

      // Header
      let header = doc.createElementNS(XHTML_NS, "div");
      header.className = "event-header devtools-toolbar";
      this.container.appendChild(header);

      if (!listener.hide.debugger) {
        let debuggerIcon = doc.createElementNS(XHTML_NS, "img");
        debuggerIcon.className = "event-tooltip-debugger-icon";
        debuggerIcon.setAttribute("src",
          "chrome://devtools/skin/images/tool-debugger.svg");
        let openInDebugger = GetStringFromName("eventsTooltip.openInDebugger");
        debuggerIcon.setAttribute("title", openInDebugger);
        header.appendChild(debuggerIcon);
      }

      if (!listener.hide.type) {
        let eventTypeLabel = doc.createElementNS(XHTML_NS, "span");
        eventTypeLabel.className = "event-tooltip-event-type";
        eventTypeLabel.textContent = listener.type;
        eventTypeLabel.setAttribute("title", listener.type);
        header.appendChild(eventTypeLabel);
      }

      if (!listener.hide.filename) {
        let filename = doc.createElementNS(XHTML_NS, "span");
        filename.className = "event-tooltip-filename devtools-monospace";
        filename.textContent = listener.origin;
        filename.setAttribute("title", listener.origin);
        header.appendChild(filename);
      }

      let attributesContainer = doc.createElementNS(XHTML_NS, "div");
      attributesContainer.className = "event-tooltip-attributes-container";
      header.appendChild(attributesContainer);

      if (!listener.hide.capturing) {
        let attributesBox = doc.createElementNS(XHTML_NS, "div");
        attributesBox.className = "event-tooltip-attributes-box";
        attributesContainer.appendChild(attributesBox);

        let capturing = doc.createElementNS(XHTML_NS, "span");
        capturing.className = "event-tooltip-attributes";
        capturing.textContent = phase;
        capturing.setAttribute("title", phase);
        attributesBox.appendChild(capturing);
      }

      if (listener.tags) {
        for (let tag of listener.tags.split(",")) {
          let attributesBox = doc.createElementNS(XHTML_NS, "div");
          attributesBox.className = "event-tooltip-attributes-box";
          attributesContainer.appendChild(attributesBox);

          let tagBox = doc.createElementNS(XHTML_NS, "span");
          tagBox.className = "event-tooltip-attributes";
          tagBox.textContent = tag;
          tagBox.setAttribute("title", tag);
          attributesBox.appendChild(tagBox);
        }
      }

      if (!listener.hide.dom0) {
        let attributesBox = doc.createElementNS(XHTML_NS, "div");
        attributesBox.className = "event-tooltip-attributes-box";
        attributesContainer.appendChild(attributesBox);

        let dom0 = doc.createElementNS(XHTML_NS, "span");
        dom0.className = "event-tooltip-attributes";
        dom0.textContent = level;
        dom0.setAttribute("title", level);
        attributesBox.appendChild(dom0);
      }

      // Content
      let content = doc.createElementNS(XHTML_NS, "div");
      let editor = new Editor(config);
      this._tooltip.eventEditors.set(content, {
        editor: editor,
        handler: listener.handler,
        searchString: listener.searchString,
        uri: listener.origin,
        dom0: listener.DOM0,
        appended: false
      });

      content.className = "event-tooltip-content-box";
      this.container.appendChild(content);

      this._addContentListeners(header);
    }

    this._tooltip.setContent(this.container, CONTAINER_WIDTH);
    this._tooltip.on("hidden", this.destroy);
  },

  _addContentListeners: function (header) {
    header.addEventListener("click", this._headerClicked);
  },

  _headerClicked: function (event) {
    if (event.target.classList.contains("event-tooltip-debugger-icon")) {
      this._debugClicked(event);
      event.stopPropagation();
      return;
    }

    let doc = this._tooltip.doc;
    let header = event.currentTarget;
    let content = header.nextElementSibling;

    if (content.hasAttribute("open")) {
      content.removeAttribute("open");
    } else {
      let contentNodes = doc.querySelectorAll(".event-tooltip-content-box");

      for (let node of contentNodes) {
        if (node !== content) {
          node.removeAttribute("open");
        }
      }

      content.setAttribute("open", "");

      let eventEditors = this._tooltip.eventEditors.get(content);

      if (eventEditors.appended) {
        return;
      }

      let {editor, handler} = eventEditors;

      let iframe = doc.createElementNS(XHTML_NS, "iframe");
      iframe.setAttribute("style", "width: 100%; height: 100%; border-style: none;");

      editor.appendTo(content, iframe).then(() => {
        let tidied = beautify.js(handler, { "indent_size": 2 });
        editor.setText(tidied);

        eventEditors.appended = true;

        let container = header.parentElement.getBoundingClientRect();
        if (header.getBoundingClientRect().top < container.top) {
          header.scrollIntoView(true);
        } else if (content.getBoundingClientRect().bottom > container.bottom) {
          content.scrollIntoView(false);
        }

        this._tooltip.emit("event-tooltip-ready");
      });
    }
  },

  _debugClicked: function (event) {
    let header = event.currentTarget;
    let content = header.nextElementSibling;

    let {uri, searchString, dom0} = this._tooltip.eventEditors.get(content);

    if (uri && uri !== "?") {
      // Save a copy of toolbox as it will be set to null when we hide the tooltip.
      let toolbox = this._toolbox;

      this._tooltip.hide();

      uri = uri.replace(/"/g, "");

      let showSource = ({ DebuggerView }) => {
        let matches = uri.match(/(.*):(\d+$)/);
        let line = 1;

        if (matches) {
          uri = matches[1];
          line = matches[2];
        }

        let item = DebuggerView.Sources.getItemForAttachment(a => a.source.url === uri);
        if (item) {
          let actor = item.attachment.source.actor;
          DebuggerView.setEditorLocation(
            actor, line, {noDebug: true}
          ).then(() => {
            if (dom0) {
              let text = DebuggerView.editor.getText();
              let index = text.indexOf(searchString);
              let lastIndex = text.lastIndexOf(searchString);

              // To avoid confusion we only search for DOM0 event handlers when
              // there is only one possible match in the file.
              if (index !== -1 && index === lastIndex) {
                text = text.substr(0, index);
                let newlineMatches = text.match(/\n/g);

                if (newlineMatches) {
                  DebuggerView.editor.setCursor({
                    line: newlineMatches.length
                  });
                }
              }
            }
          });
        }
      };

      let debuggerAlreadyOpen = toolbox.getPanel("jsdebugger");
      toolbox.selectTool("jsdebugger").then(({ panelWin: dbg }) => {
        if (debuggerAlreadyOpen) {
          showSource(dbg);
        } else {
          dbg.once(dbg.EVENTS.SOURCES_ADDED, () => showSource(dbg));
        }
      });
    }
  },

  destroy: function () {
    if (this._tooltip) {
      this._tooltip.off("hidden", this.destroy);

      let boxes = this.container.querySelectorAll(".event-tooltip-content-box");

      for (let box of boxes) {
        let {editor} = this._tooltip.eventEditors.get(box);
        editor.destroy();
      }

      this._tooltip.eventEditors = null;
    }

    let headerNodes = this.container.querySelectorAll(".event-header");

    for (let node of headerNodes) {
      node.removeEventListener("click", this._headerClicked);
    }

    let sourceNodes = this.container.querySelectorAll(".event-tooltip-debugger-icon");
    for (let node of sourceNodes) {
      node.removeEventListener("click", this._debugClicked);
    }

    this._eventListenerInfos = this._toolbox = this._tooltip = null;
  }
};

module.exports.setEventTooltip = setEventTooltip;
