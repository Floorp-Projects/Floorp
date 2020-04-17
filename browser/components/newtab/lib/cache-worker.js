/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

/* global ReactDOMServer, NewtabRenderUtils */

const PAGE_TEMPLATE_RESOURCE_PATH =
  "resource://activity-stream/data/content/abouthomecache/page.html.template";
const SCRIPT_TEMPLATE_RESOURCE_PATH =
  "resource://activity-stream/data/content/abouthomecache/script.js.template";

// If we don't stub these functions out, React throws warnings in the console
// upon being loaded.
let window = self;
window.requestAnimationFrame = () => {};
window.cancelAnimationFrame = () => {};

importScripts("resource://gre/modules/workers/require.js");

{
  let oldChromeUtils = ChromeUtils;

  // ChromeUtils is defined inside of a Worker, but we don't want the
  // activity-stream.bundle.js to detect it when loading, since that results
  // in it attempting to import `Services.jsm` on load, which is not allowed in
  // a Worker. So we temporarily clear ChromeUtils so that activity-stream.bundle.js
  // thinks its being loaded in content scope.
  //
  // eslint-disable-next-line no-global-assign
  ChromeUtils = undefined;

  importScripts(
    "resource://activity-stream/vendor/react.js",
    "resource://activity-stream/vendor/react-dom.js",
    "resource://activity-stream/vendor/react-dom-server.js",
    "resource://activity-stream/vendor/redux.js",
    "resource://activity-stream/vendor/react-transition-group.js",
    "resource://activity-stream/vendor/prop-types.js",
    "resource://activity-stream/vendor/react-redux.js",
    "resource://activity-stream/data/content/activity-stream.bundle.js"
  );

  // eslint-disable-next-line no-global-assign
  ChromeUtils = oldChromeUtils;
}

let PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

let Agent = {
  _templates: null,

  /**
   * Synchronously loads the template files off of the file
   * system, and returns them as an object. If the Worker has loaded
   * these templates before, a cached copy of the templates is returned
   * instead.
   *
   * @return Object
   *   An object with the following properties:
   *
   *   pageTemplate (String):
   *     The template for the document markup.
   *
   *   scriptTempate (String):
   *     The template for the script.
   */
  getOrCreateTemplates() {
    if (this._templates) {
      return this._templates;
    }

    const templateResources = new Map([
      ["pageTemplate", PAGE_TEMPLATE_RESOURCE_PATH],
      ["scriptTemplate", SCRIPT_TEMPLATE_RESOURCE_PATH],
    ]);

    this._templates = {};

    for (let [name, path] of templateResources) {
      const xhr = new XMLHttpRequest();
      // Using a synchronous XHR in a worker is fine.
      xhr.open("GET", path, false);
      xhr.responseType = "text";
      xhr.send(null);
      this._templates[name] = xhr.responseText;
    }

    return this._templates;
  },

  /**
   * Constructs the cached about:home document using ReactDOMServer.
   *
   * @param state (Object)
   *   The most recent Activity Stream Redux state.
   * @return Object
   *   An object with the following properties:
   *
   *   page (String):
   *     The generated markup for the document.
   *
   *   script (String):
   *     The generated script for the document.
   */
  construct(state) {
    state.App.isForStartupCache = true;

    // ReactDOMServer.renderToString expects a Redux store to pull
    // the state from, so we mock out a minimal store implementation.
    let fakeStore = {
      getState() {
        return state;
      },
      dispatch() {},
    };

    let markup = ReactDOMServer.renderToString(
      NewtabRenderUtils.NewTab({
        store: fakeStore,
        isFirstrun: false,
      })
    );

    let { pageTemplate, scriptTemplate } = this.getOrCreateTemplates();
    let cacheTime = new Date().toUTCString();
    let page = pageTemplate
      .replace("{{ MARKUP }}", markup)
      .replace("{{ CACHE_TIME }}", cacheTime);
    let script = scriptTemplate.replace(
      "{{ STATE }}",
      JSON.stringify(state, null, "\t")
    );

    return { page, script };
  },
};

// This boilerplate connects the PromiseWorker to the Agent so
// that messages from the main thread map to methods on the
// Agent.
let worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function(method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function(result, ...transfers) {
  self.postMessage(result, ...transfers);
};
worker.close = function() {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));
