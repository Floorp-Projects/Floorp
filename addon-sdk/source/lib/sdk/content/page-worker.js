/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { frames } = require("../remote/child");
const { Class } = require("../core/heritage");
const { Disposable } = require('../core/disposable');
const { data } = require("../self");
const { once } = require("../dom/events");
const { getAttachEventType } = require("./utils");
const { Rules } = require('../util/rules');
const { uuid } = require('../util/uuid');
const { WorkerChild } = require("./worker-child");
const { Cc, Ci, Cu } = require("chrome");
const { on: onSystemEvent } = require("../system/events");

const appShell = Cc["@mozilla.org/appshell/appShellService;1"].getService(Ci.nsIAppShellService);

const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

const pages = new Map();

const DOC_INSERTED = "document-element-inserted";

function isValidURL(page, url) {
  return !page.rules || page.rules.matchesAny(url);
}

const ChildPage = Class({
  implements: [ Disposable ],
  setup: function(frame, id, options) {
    this.id = id;
    this.frame = frame;
    this.options = options;

    this.webNav = appShell.createWindowlessBrowser(false);
    this.docShell.allowJavascript = this.options.allow.script;

    // Accessing the browser's window forces the initial about:blank document to
    // be created before we start listening for notifications
    this.contentWindow;

    this.webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);

    pages.set(this.id, this);

    this.contentURL = options.contentURL;

    if (options.include) {
      this.rules = Rules();
      this.rules.add.apply(this.rules, [].concat(options.include));
    }
  },

  dispose: function() {
    pages.delete(this.id);
    this.webProgress.removeProgressListener(this);
    this.webNav.close();
    this.webNav = null;
  },

  attachWorker: function() {
    if (!isValidURL(this, this.contentWindow.location.href))
      return;

    this.options.id = uuid().toString();
    this.options.window = this.contentWindow;
    this.frame.port.emit("sdk/frame/connect", this.id, {
      id: this.options.id,
      url: this.contentWindow.document.documentURIObject.spec
    });
    new WorkerChild(this.options);
  },

  get docShell() {
    return this.webNav.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDocShell);
  },

  get webProgress() {
    return this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebProgress);
  },

  get contentWindow() {
    return this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow);
  },

  get contentURL() {
    return this.options.contentURL;
  },
  set contentURL(url) {
    this.options.contentURL = url;

    url = this.options.contentURL ? data.url(this.options.contentURL) : "about:blank";

    this.webNav.loadURI(url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
  },

  onLocationChange: function(progress, request, location, flags) {
    // Ignore inner-frame events
    if (progress != this.webProgress)
      return;
    // Ignore events that don't change the document
    if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)
      return;

    let event = getAttachEventType(this.options);
    // Attaching at the start of the load is handled by the
    // document-element-inserted listener.
    if (event == DOC_INSERTED)
      return;

    once(this.contentWindow, event, () => {
      this.attachWorker();
    }, false);
  },

  QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener", "nsISupportsWeakReference"])
});

onSystemEvent(DOC_INSERTED, ({type, subject, data}) => {
  let page = Array.from(pages.values()).find(p => p.contentWindow.document === subject);

  if (!page)
    return;

  if (getAttachEventType(page.options) == DOC_INSERTED)
    page.attachWorker();
}, true);

frames.port.on("sdk/frame/create", (frame, id, options) => {
  new ChildPage(frame, id, options);
});

frames.port.on("sdk/frame/set", (frame, id, params) => {
  let page = pages.get(id);
  if (!page)
    return;

  if ("allowScript" in params)
    page.docShell.allowJavascript = params.allowScript;
  if ("contentURL" in params)
    page.contentURL = params.contentURL;
});

frames.port.on("sdk/frame/destroy", (frame, id) => {
  let page = pages.get(id);
  if (!page)
    return;

  page.destroy();
});
