/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const { Worker } = require('./traits-worker');
const { Loader } = require('../content/loader');
const hiddenFrames = require('../frame/hidden-frame');
const { on, off } = require('../system/events');
const unload = require('../system/unload');
const { getDocShell } = require("../frame/utils");
const { ignoreWindow } = require('../private-browsing/utils');

// Everything coming from add-on's xpi considered an asset.
const assetsURI = require('../self').data.url().replace(/data\/$/, "");

/**
 * This trait is layered on top of `Worker` and in contrast to symbiont
 * Worker constructor requires `content` option that represents content
 * that will be loaded in the provided frame, if frame is not provided
 * Worker will create hidden one.
 */
const Symbiont = Worker.resolve({
    constructor: '_initWorker',
    destroy: '_workerDestroy'
  }).compose(Loader, {

  /**
   * The constructor requires all the options that are required by
   * `require('content').Worker` with the difference that the `frame` option
   * is optional. If `frame` is not provided, `contentURL` is expected.
   * @param {Object} options
   * @param {String} options.contentURL
   *    URL of a content to load into `this._frame` and create worker for.
   * @param {Element} [options.frame]
   *    iframe element that is used to load `options.contentURL` into.
   *    if frame is not provided hidden iframe will be created.
   */
  constructor: function Symbiont(options) {
    options = options || {};

    if ('contentURL' in options)
        this.contentURL = options.contentURL;
    if ('contentScriptWhen' in options)
      this.contentScriptWhen = options.contentScriptWhen;
    if ('contentScriptOptions' in options)
      this.contentScriptOptions = options.contentScriptOptions;
    if ('contentScriptFile' in options)
      this.contentScriptFile = options.contentScriptFile;
    if ('contentScript' in options)
      this.contentScript = options.contentScript;
    if ('allow' in options)
      this.allow = options.allow;
    if ('onError' in options)
        this.on('error', options.onError);
    if ('onMessage' in options)
        this.on('message', options.onMessage);
    if ('frame' in options) {
      this._initFrame(options.frame);
    }
    else {
      let self = this;
      this._hiddenFrame = hiddenFrames.HiddenFrame({
        onReady: function onFrame() {
          self._initFrame(this.element);
        },
        onUnload: function onUnload() {
          // Bug 751211: Remove reference to _frame when hidden frame is
          // automatically removed on unload, otherwise we are going to face
          // "dead object" exception
          self.destroy();
        }
      });
      hiddenFrames.add(this._hiddenFrame);
    }

    unload.ensure(this._public, "destroy");
  },

  destroy: function destroy() {
    this._workerDestroy();
    this._unregisterListener();
    this._frame = null;
    if (this._hiddenFrame) {
      hiddenFrames.remove(this._hiddenFrame);
      this._hiddenFrame = null;
    }
  },

  /**
   * XUL iframe or browser elements with attribute `type` being `content`.
   * Used to create `ContentSymbiont` from.
   * @type {nsIFrame|nsIBrowser}
   */
  _frame: null,

  /**
   * Listener to the `'frameReady"` event (emitted when `iframe` is ready).
   * Removes listener, sets right permissions to the frame and loads content.
   */
  _initFrame: function _initFrame(frame) {
    if (this._loadListener)
      this._unregisterListener();

    this._frame = frame;

    if (getDocShell(frame)) {
      this._reallyInitFrame(frame);
    }
    else {
      if (this._waitForFrame) {
        off('content-document-global-created', this._waitForFrame);
      }
      this._waitForFrame = this.__waitForFrame.bind(this, frame);
      on('content-document-global-created', this._waitForFrame);
    }
  },

  __waitForFrame: function _waitForFrame(frame, { subject: win }) {
    if (frame.contentWindow == win) {
      off('content-document-global-created', this._waitForFrame);
      delete this._waitForFrame;
      this._reallyInitFrame(frame);
    }
  },

  _reallyInitFrame: function _reallyInitFrame(frame) {
    getDocShell(frame).allowJavascript = this.allow.script;
    frame.setAttribute("src", this._contentURL);

    // Inject `addon` object in document if we load a document from
    // one of our addon folder and if no content script are defined. bug 612726
    let isDataResource =
      typeof this._contentURL == "string" &&
      this._contentURL.indexOf(assetsURI) == 0;
    let hasContentScript =
      (Array.isArray(this.contentScript) ? this.contentScript.length > 0
                                             : !!this.contentScript) ||
      (Array.isArray(this.contentScriptFile) ? this.contentScriptFile.length > 0
                                             : !!this.contentScriptFile);
    // If we have to inject `addon` we have to do it before document
    // script execution, so during `start`:
    this._injectInDocument = isDataResource && !hasContentScript;
    if (this._injectInDocument)
      this.contentScriptWhen = "start";

    if ((frame.contentDocument.readyState == "complete" ||
        (frame.contentDocument.readyState == "interactive" &&
         this.contentScriptWhen != 'end' )) &&
        frame.contentDocument.location == this._contentURL) {
      // In some cases src doesn't change and document is already ready
      // (for ex: when the user moves a widget while customizing toolbars.)
      this._onInit();
      return;
    }

    let self = this;

    if ('start' == this.contentScriptWhen) {
      this._loadEvent = 'start';
      on('document-element-inserted',
        this._loadListener = function onStart({ subject: doc }) {
          let window = doc.defaultView;

          if (ignoreWindow(window)) {
            return;
          }

          if (window && window == frame.contentWindow) {
            self._unregisterListener();
            self._onInit();
          }

        });
      return;
    }

    let eventName = 'end' == this.contentScriptWhen ? 'load' : 'DOMContentLoaded';
    let self = this;
    this._loadEvent = eventName;
    frame.addEventListener(eventName,
      this._loadListener = function _onReady(event) {

        if (event.target != frame.contentDocument)
          return;
        self._unregisterListener();

        self._onInit();

      }, true);

  },

  /**
   * Unregister listener that watchs for document being ready to be injected.
   * This listener is registered in `Symbiont._initFrame`.
   */
  _unregisterListener: function _unregisterListener() {
    if (this._waitForFrame) {
      off('content-document-global-created', this._waitForFrame);
      delete this._waitForFrame;
    }

    if (!this._loadListener)
      return;
    if (this._loadEvent == "start") {
      off('document-element-inserted', this._loadListener);
    }
    else {
      this._frame.removeEventListener(this._loadEvent, this._loadListener,
                                      true);
    }
    this._loadListener = null;
  },

  /**
   * Called by Symbiont itself when the frame is ready to load
   * content scripts according to contentScriptWhen. Overloaded by Panel.
   */
  _onInit: function () {
    this._initWorker({ window: this._frame.contentWindow });
  }

});
exports.Symbiont = Symbiont;
