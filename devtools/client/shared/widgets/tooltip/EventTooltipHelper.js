/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/inspector.properties"
);

const Editor = require("resource://devtools/client/shared/sourceeditor/editor.js");
const beautify = require("resource://devtools/shared/jsbeautify/beautify.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const CONTAINER_WIDTH = 500;

const L10N_BUBBLING = L10N.getStr("eventsTooltip.Bubbling");
const L10N_CAPTURING = L10N.getStr("eventsTooltip.Capturing");

class EventTooltip extends EventEmitter {
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
   * @param {NodeFront} nodeFront
   *        The nodeFront we're displaying event listeners for.
   */
  constructor(tooltip, eventListenerInfos, toolbox, nodeFront) {
    super();

    this._tooltip = tooltip;
    this._toolbox = toolbox;
    this._eventEditors = new WeakMap();
    this._nodeFront = nodeFront;
    this._eventListenersAbortController = new AbortController();

    // Used in tests: add a reference to the EventTooltip instance on the HTMLTooltip.
    this._tooltip.eventTooltip = this;

    this._headerClicked = this._headerClicked.bind(this);
    this._eventToggleCheckboxChanged =
      this._eventToggleCheckboxChanged.bind(this);

    this._subscriptions = [];

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
    this.container = doc.createElementNS(XHTML_NS, "ul");
    this.container.className = "devtools-tooltip-events-container";

    const sourceMapURLService = this._toolbox.sourceMapURLService;

    for (let i = 0; i < eventListenerInfos.length; i++) {
      const listener = eventListenerInfos[i];

      // Create this early so we can refer to it from a closure, below.
      const content = doc.createElementNS(XHTML_NS, "div");
      const codeMirrorContainerId = `cm-${i}`;
      content.id = codeMirrorContainerId;

      // Header
      const header = doc.createElementNS(XHTML_NS, "div");
      header.className = "event-header";
      header.setAttribute("data-event-type", listener.type);

      const arrow = doc.createElementNS(XHTML_NS, "button");
      arrow.className = "theme-twisty";
      arrow.setAttribute("aria-expanded", "false");
      arrow.setAttribute("aria-owns", codeMirrorContainerId);
      arrow.setAttribute(
        "title",
        L10N.getFormatStr("eventsTooltip.toggleButton.label", listener.type)
      );

      header.appendChild(arrow);

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
                this._tooltip.emitForTests("event-tooltip-source-map-ready");
              }
            )
          );
        }
      }

      filename.textContent = text;
      filename.setAttribute("title", title);
      header.appendChild(filename);

      if (!listener.hide.debugger) {
        const debuggerIcon = doc.createElementNS(XHTML_NS, "button");
        debuggerIcon.className = "event-tooltip-debugger-icon";
        const openInDebugger = L10N.getFormatStr(
          "eventsTooltip.openInDebugger2",
          listener.type
        );
        debuggerIcon.setAttribute("title", openInDebugger);
        header.appendChild(debuggerIcon);
      }

      const attributesContainer = doc.createElementNS(XHTML_NS, "div");
      attributesContainer.className = "event-tooltip-attributes-container";
      header.appendChild(attributesContainer);

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

      if (!listener.hide.capturing) {
        const attributesBox = doc.createElementNS(XHTML_NS, "div");
        attributesBox.className = "event-tooltip-attributes-box";
        attributesContainer.appendChild(attributesBox);

        const capturing = doc.createElementNS(XHTML_NS, "span");
        capturing.className = "event-tooltip-attributes";

        const phase = listener.capturing ? L10N_CAPTURING : L10N_BUBBLING;
        capturing.textContent = phase;
        capturing.setAttribute("title", phase);
        attributesBox.appendChild(capturing);
      }

      const toggleListenerCheckbox = doc.createElementNS(XHTML_NS, "input");
      toggleListenerCheckbox.type = "checkbox";
      toggleListenerCheckbox.className =
        "event-tooltip-listener-toggle-checkbox";
      toggleListenerCheckbox.setAttribute(
        "aria-label",
        L10N.getFormatStr("eventsTooltip.toggleListenerLabel", listener.type)
      );
      if (listener.eventListenerInfoId) {
        toggleListenerCheckbox.checked = listener.enabled;
        toggleListenerCheckbox.setAttribute(
          "data-event-listener-info-id",
          listener.eventListenerInfoId
        );
        toggleListenerCheckbox.addEventListener(
          "change",
          this._eventToggleCheckboxChanged,
          { signal: this._eventListenersAbortController.signal }
        );
      } else {
        toggleListenerCheckbox.checked = true;
        toggleListenerCheckbox.setAttribute("disabled", true);
      }
      header.appendChild(toggleListenerCheckbox);

      // Content
      const editor = new Editor(config);
      this._eventEditors.set(content, {
        editor,
        handler: listener.handler,
        native: listener.native,
        appended: false,
        location,
      });

      content.className = "event-tooltip-content-box";

      const li = doc.createElementNS(XHTML_NS, "li");
      li.append(header, content);
      this.container.appendChild(li);
      this._addContentListeners(header);
    }

    this._tooltip.panel.innerHTML = "";
    this._tooltip.panel.appendChild(this.container);
    this._tooltip.setContentSize({ width: CONTAINER_WIDTH, height: Infinity });
  }

  _addContentListeners(header) {
    header.addEventListener("click", this._headerClicked, {
      signal: this._eventListenersAbortController.signal,
    });
  }

  _headerClicked(event) {
    // Clicking on the checkbox shouldn't impact the header (checkbox state change is
    // handled in _eventToggleCheckboxChanged).
    if (
      event.target.classList.contains("event-tooltip-listener-toggle-checkbox")
    ) {
      event.stopPropagation();
      return;
    }

    if (event.target.classList.contains("event-tooltip-debugger-icon")) {
      this._debugClicked(event);
      event.stopPropagation();
      return;
    }

    const doc = this._tooltip.doc;
    const header = event.currentTarget;
    const content = header.nextElementSibling;
    const twisty = header.querySelector(".theme-twisty");

    if (content.hasAttribute("open")) {
      header.classList.remove("content-expanded");
      twisty.setAttribute("aria-expanded", false);
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
        const nodeTwisty = node.querySelector(".theme-twisty");
        nodeTwisty.setAttribute("aria-expanded", false);
      }
      for (const node of openContent) {
        node.removeAttribute("open");
      }

      header.classList.add("content-expanded");
      content.setAttribute("open", "");
      twisty.setAttribute("aria-expanded", true);

      const eventEditor = this._eventEditors.get(content);

      if (eventEditor.appended) {
        return;
      }

      const { editor, handler } = eventEditor;

      const iframe = doc.createElementNS(XHTML_NS, "iframe");
      iframe.classList.add("event-tooltip-editor-frame");
      iframe.setAttribute(
        "title",
        L10N.getFormatStr(
          "eventsTooltip.codeIframeTitle",
          header.getAttribute("data-event-type")
        )
      );

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

        this._tooltip.emitForTests("event-tooltip-ready");
      });
    }
  }

  _debugClicked(event) {
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
  }

  async _eventToggleCheckboxChanged(event) {
    const checkbox = event.currentTarget;
    const id = checkbox.getAttribute("data-event-listener-info-id");
    if (checkbox.checked) {
      await this._nodeFront.enableEventListener(id);
    } else {
      await this._nodeFront.disableEventListener(id);
    }
    this.emit("event-tooltip-listener-toggled", {
      hasDisabledEventListeners:
        // No need to query the other checkboxes if the handled checkbox is unchecked
        !checkbox.checked ||
        this._tooltip.doc.querySelector(
          `input.event-tooltip-listener-toggle-checkbox:not(:checked)`
        ) !== null,
    });
  }

  /**
   * Parse URI and return {url, line, column}; or return null if it can't be parsed.
   */
  _parseLocation(uri) {
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
  }

  destroy() {
    if (this._tooltip) {
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

    this.clearEvents();
    if (this._eventListenersAbortController) {
      this._eventListenersAbortController.abort();
      this._eventListenersAbortController = null;
    }

    for (const unsubscribe of this._subscriptions) {
      unsubscribe();
    }

    this._toolbox = this._tooltip = this._nodeFront = null;
  }
}

module.exports.EventTooltip = EventTooltip;
