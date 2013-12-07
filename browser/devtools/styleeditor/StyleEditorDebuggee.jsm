/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StyleEditorDebuggee", "StyleSheet"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "promise",
    "resource://gre/modules/commonjs/sdk/core/promise.js", "Promise");

/**
 * A StyleEditorDebuggee represents the document the style editor is debugging.
 * It maintains a list of StyleSheet objects that represent the stylesheets in
 * the target's document. It wraps remote debugging protocol comunications.
 *
 * It emits these events:
 *   'document-load': debuggee's document is loaded, style sheets are argument
 *   'stylesheets-cleared': The debuggee's stylesheets have been reset (e.g. the
 *                          page navigated)
 *
 * @param {Target} target
 *         The target the debuggee is listening to
 */
let StyleEditorDebuggee = function(target) {
  EventEmitter.decorate(this);

  this.styleSheets = [];

  this.clear = this.clear.bind(this);
  this._onNewDocument = this._onNewDocument.bind(this);
  this._onDocumentLoad = this._onDocumentLoad.bind(this);

  this._target = target;
  this._actor = this.target.form.styleEditorActor;

  this.client.addListener("documentLoad", this._onDocumentLoad);
  this._target.on("navigate", this._onNewDocument);

  this._onNewDocument();
}

StyleEditorDebuggee.prototype = {
  /**
   * list of StyleSheet objects for this target
   */
  styleSheets: null,

  /**
   * baseURIObject for the current document
   */
  baseURI: null,

  /**
   * The target we're debugging
   */
  get target() {
    return this._target;
  },

  /**
   * Client for communicating with server with remote debug protocol.
   */
  get client() {
    return this._target.client;
  },

  /**
   * Get the StyleSheet object with the given href.
   *
   * @param  {string} href
   *         Url of the stylesheet to find
   * @return {StyleSheet}
   *         StyleSheet with the matching href
   */
  styleSheetFromHref: function(href) {
    for (let sheet of this.styleSheets) {
      if (sheet.href == href) {
        return sheet;
      }
    }
    return null;
  },

  /**
   * Clear stylesheets and state.
   */
  clear: function() {
    this.baseURI = null;
    this.clearStyleSheets();
  },

  /**
   * Clear stylesheets.
   */
  clearStyleSheets: function() {
    for (let stylesheet of this.styleSheets) {
      stylesheet.destroy();
    }
    this.styleSheets = [];
    this.emit("stylesheets-cleared");
  },

  /**
   * Called when target is created or has navigated.
   * Clear previous sheets and request new document's
   */
  _onNewDocument: function() {
    this.clear();

    this._getBaseURI();

    let message = { type: "newDocument" };
    this._sendRequest(message);
  },

  /**
   * request baseURIObject information from the document
   */
  _getBaseURI: function() {
    let message = { type: "getBaseURI" };
    this._sendRequest(message, (response) => {
      this.baseURI = Services.io.newURI(response.baseURI, null, null);
    });
  },

  /**
   * Handler for document load, forward event with
   * all the stylesheets available on load.
   *
   * @param  {string} type
   *         Event type
   * @param  {object} request
   *         Object with 'styleSheets' array of actor forms
   */
  _onDocumentLoad: function(type, request) {
    if (this.styleSheets.length > 0) {
      this.clearStyleSheets();
    }
    let sheets = [];
    for (let form of request.styleSheets) {
      let sheet = this._addStyleSheet(form);
      sheets.push(sheet);
    }
    this.emit("document-load", sheets);
  },

  /**
   * Create a new StyleSheet object from the form
   * and add to our stylesheet list.
   *
   * @param {object} form
   *        Initial properties of the stylesheet
   */
  _addStyleSheet: function(form) {
    let sheet = new StyleSheet(form, this);
    this.styleSheets.push(sheet);
    return sheet;
  },

  /**
   * Create a new stylesheet with the given text
   * and attach it to the document.
   *
   * @param {string} text
   *        Initial text of the stylesheet
   * @param {function} callback
   *        Function to call when the stylesheet has been added to the document
   */
  createStyleSheet: function(text, callback) {
    let message = { type: "newStyleSheet", text: text };
    this._sendRequest(message, (response) => {
      let sheet = this._addStyleSheet(response.styleSheet);
      callback(sheet);
    });
  },

  /**
   * Send a request to our actor on the server
   *
   * @param {object} message
   *        Message to send to the actor
   * @param {function} callback
   *        Function to call with reponse from actor
   */
  _sendRequest: function(message, callback) {
    message.to = this._actor;
    this.client.request(message, callback);
  },

  /**
   * Clean up and remove listeners
   */
  destroy: function() {
    this.clear();

    this._target.off("navigate", this._onNewDocument);
  }
}

/**
 * A StyleSheet object represents a stylesheet on the debuggee. It wraps
 * communication with a complimentary StyleSheetActor on the server.
 *
 * It emits these events:
 *   'source-load' - The full text source of the stylesheet has been fetched
 *   'property-change' - Any property (e.g 'disabled') has changed
 *   'style-applied' - A change has been applied to the live stylesheet on the server
 *   'error' - An error occured when loading or saving stylesheet
 *
 * @param {object} form
 *        Initial properties of the stylesheet
 * @param {StyleEditorDebuggee} debuggee
 *        Owner of the stylesheet
 */
let StyleSheet = function(form, debuggee) {
  EventEmitter.decorate(this);

  this.debuggee = debuggee;
  this._client = debuggee.client;
  this._actor = form.actor;

  this._onSourceLoad = this._onSourceLoad.bind(this);
  this._onPropertyChange = this._onPropertyChange.bind(this);
  this._onStyleApplied = this._onStyleApplied.bind(this);

  this._client.addListener("sourceLoad", this._onSourceLoad);
  this._client.addListener("propertyChange", this._onPropertyChange);
  this._client.addListener("styleApplied", this._onStyleApplied);

  // Backwards compatibility
  this._client.addListener("sourceLoad-" + this._actor, this._onSourceLoad);
  this._client.addListener("propertyChange-" + this._actor, this._onPropertyChange);
  this._client.addListener("styleApplied-" + this._actor, this._onStyleApplied);


  // set initial property values
  for (let attr in form) {
    this[attr] = form[attr];
  }
}

StyleSheet.prototype = {
  /**
   * Toggle the disabled attribute of the stylesheet
   */
  toggleDisabled: function() {
    let message = { type: "toggleDisabled" };
    this._sendRequest(message);
  },

  /**
   * Request that the source of the stylesheet be fetched.
   * 'source-load' event will be fired when it's been fetched.
   */
  fetchSource: function() {
    let message = { type: "fetchSource" };
    this._sendRequest(message);
  },

  /**
   * Update the stylesheet in place with the given full source.
   *
   * @param {string} sheetText
   *        Full text to update the stylesheet with
   */
  update: function(sheetText) {
    let message = { type: "update", text: sheetText, transition: true };
    this._sendRequest(message);
  },

  /**
   * Handle source load event from the client.
   *
   * @param {string} type
   *        Event type
   * @param {object} request
   *        Event details
   */
  _onSourceLoad: function(type, request) {
    if (request.from == this._actor) {
      if (request.error) {
        return this.emit("error", request.error);
      }
      this.emit("source-load", request.source);
    }
  },

  /**
   * Handle a property change on the stylesheet
   *
   * @param {string} type
   *        Event type
   * @param {object} request
   *        Event details
   */
  _onPropertyChange: function(type, request) {
    if (request.from == this._actor) {
      this[request.property] = request.value;
      this.emit("property-change", request.property);
    }
  },

  /**
   * Handle event when update has been successfully applied and propogate it.
   */
  _onStyleApplied: function(type, request) {
    if (request.from == this._actor) {
      this.emit("style-applied");
    }
  },

  /**
   * Send a request to our actor on the server
   *
   * @param {object} message
   *        Message to send to the actor
   * @param {function} callback
   *        Function to call with reponse from actor
   */
  _sendRequest: function(message, callback) {
    message.to = this._actor;
    this._client.request(message, callback);
  },

  /**
   * Clean up and remove event listeners
   */
  destroy: function() {
    this._client.removeListener("sourceLoad", this._onSourceLoad);
    this._client.removeListener("propertyChange", this._onPropertyChange);
    this._client.removeListener("styleApplied", this._onStyleApplied);

    this._client.removeListener("sourceLoad-" + this._actor, this._onSourceLoad);
    this._client.removeListener("propertyChange-" + this._actor, this._onPropertyChange);
    this._client.removeListener("styleApplied-" + this._actor, this._onStyleApplied);
  }
}
