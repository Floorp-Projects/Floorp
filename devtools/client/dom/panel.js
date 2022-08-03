/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");

const EventEmitter = require("devtools/shared/event-emitter");
loader.lazyRequireGetter(
  this,
  "openContentLink",
  "devtools/client/shared/link",
  true
);

/**
 * This object represents DOM panel. It's responsibility is to
 * render Document Object Model of the current debugger target.
 */
function DomPanel(iframeWindow, toolbox, commands) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._commands = commands;

  this.onContentMessage = this.onContentMessage.bind(this);
  this.onPanelVisibilityChange = this.onPanelVisibilityChange.bind(this);

  this.pendingRequests = new Map();

  EventEmitter.decorate(this);
}

DomPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the DOM panel completes opening.
   */
  async open() {
    // Wait for the retrieval of root object properties before resolving open
    const onGetProperties = new Promise(resolve => {
      this._resolveOpen = resolve;
    });

    await this.initialize();

    await onGetProperties;

    return this;
  },

  // Initialization

  async initialize() {
    this.panelWin.addEventListener(
      "devtools/content/message",
      this.onContentMessage,
      true
    );

    this._toolbox.on("select", this.onPanelVisibilityChange);

    // onTargetAvailable is mandatory when calling watchTargets
    this._onTargetAvailable = () => {};
    this._onTargetSelected = this._onTargetSelected.bind(this);
    await this._commands.targetCommand.watchTargets({
      types: [this._commands.targetCommand.TYPES.FRAME],
      onAvailable: this._onTargetAvailable,
      onSelected: this._onTargetSelected,
    });

    this.onResourceAvailable = this.onResourceAvailable.bind(this);
    await this._commands.resourceCommand.watchResources(
      [this._commands.resourceCommand.TYPES.DOCUMENT_EVENT],
      {
        onAvailable: this.onResourceAvailable,
      }
    );

    // Export provider object with useful API for DOM panel.
    const provider = {
      getToolbox: this.getToolbox.bind(this),
      getPrototypeAndProperties: this.getPrototypeAndProperties.bind(this),
      openLink: this.openLink.bind(this),
      // Resolve DomPanel.open once the object properties are fetched
      onPropertiesFetched: () => {
        if (this._resolveOpen) {
          this._resolveOpen();
          this._resolveOpen = null;
        }
      },
    };

    exportIntoContentScope(this.panelWin, provider, "DomProvider");
  },

  destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this._commands.targetCommand.unwatchTargets({
      types: [this._commands.targetCommand.TYPES.FRAME],
      onAvailable: this._onTargetAvailable,
      onSelected: this._onTargetSelected,
    });
    this._commands.resourceCommand.unwatchResources(
      [this._commands.resourceCommand.TYPES.DOCUMENT_EVENT],
      { onAvailable: this.onResourceAvailable }
    );
    this._toolbox.off("select", this.onPanelVisibilityChange);

    this.emit("destroyed");
  },

  // Events

  refresh() {
    // Do not refresh if the panel isn't visible.
    if (!this.isPanelVisible()) {
      return;
    }

    // Do not refresh if it isn't necessary.
    if (!this.shouldRefresh) {
      return;
    }

    // Alright reset the flag we are about to refresh the panel.
    this.shouldRefresh = false;

    this.getRootGrip().then(rootGrip => {
      this.postContentMessage("initialize", rootGrip);
    });
  },

  /**
   * Make sure the panel is refreshed, either when navigation occurs or when a frame is
   * selected in the iframe picker.
   * The panel is refreshed immediately if it's currently selected or lazily when the user
   * actually selects it.
   */
  forceRefresh() {
    this.shouldRefresh = true;
    // This will end up calling scriptCommand execute method to retrieve the `window` grip
    // on targetCommand.selectedTargetFront.
    this.refresh();
  },

  _onTargetSelected({ targetFront }) {
    this.forceRefresh();
  },

  onResourceAvailable(resources) {
    for (const resource of resources) {
      // Only consider top level document, and ignore remote iframes top document
      if (
        resource.resourceType ===
          this._commands.resourceCommand.TYPES.DOCUMENT_EVENT &&
        resource.name === "dom-complete" &&
        resource.targetFront.isTopLevel
      ) {
        this.forceRefresh();
      }
    }
  },

  /**
   * Make sure the panel is refreshed (if needed) when it's selected.
   */
  onPanelVisibilityChange() {
    this.refresh();
  },

  // Helpers

  /**
   * Return true if the DOM panel is currently selected.
   */
  isPanelVisible() {
    return this._toolbox.currentToolId === "dom";
  },

  async getPrototypeAndProperties(objectFront) {
    if (!objectFront.actorID) {
      console.error("No actor!", objectFront);
      throw new Error("Failed to get object front.");
    }

    // Bail out if target doesn't exist (toolbox maybe closed already).
    if (!this.currentTarget) {
      return null;
    }

    // Check for a previously stored request for grip.
    let request = this.pendingRequests.get(objectFront.actorID);

    // If no request is in progress create a new one.
    if (!request) {
      request = objectFront.getPrototypeAndProperties();
      this.pendingRequests.set(objectFront.actorID, request);
    }

    const response = await request;
    this.pendingRequests.delete(objectFront.actorID);

    // Fire an event about not having any pending requests.
    if (!this.pendingRequests.size) {
      this.emit("no-pending-requests");
    }

    return response;
  },

  openLink(url) {
    openContentLink(url);
  },

  async getRootGrip() {
    const { result } = await this._toolbox.commands.scriptCommand.execute(
      "window"
    );
    return result;
  },

  postContentMessage(type, args) {
    const data = {
      type,
      args,
    };

    const event = new this.panelWin.MessageEvent("devtools/chrome/message", {
      bubbles: true,
      cancelable: true,
      data,
    });

    this.panelWin.dispatchEvent(event);
  },

  onContentMessage(event) {
    const data = event.data;
    const method = data.type;
    if (typeof this[method] == "function") {
      this[method](data.args);
    }
  },

  getToolbox() {
    return this._toolbox;
  },

  get currentTarget() {
    return this._toolbox.target;
  },
};

// Helpers

function exportIntoContentScope(win, obj, defineAs) {
  const clone = Cu.createObjectIn(win, {
    defineAs,
  });

  const props = Object.getOwnPropertyNames(obj);
  for (let i = 0; i < props.length; i++) {
    const propName = props[i];
    const propValue = obj[propName];
    if (typeof propValue == "function") {
      Cu.exportFunction(propValue, clone, {
        defineAs: propName,
      });
    }
  }
}

// Exports from this module
exports.DomPanel = DomPanel;
