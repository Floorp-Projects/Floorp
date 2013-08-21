/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const { Class } = require('./core/heritage');
const { on, emit, off, setListeners } = require('./event/core');
const { filter, pipe, map, merge: streamMerge } = require('./event/utils');
const { WorkerHost, Worker, detach, attach } = require('./worker/utils');
const { Disposable } = require('./core/disposable');
const { EventTarget } = require('./event/target');
const { unload } = require('./system/unload');
const { events, streamEventsFrom } = require('./content/events');
const { getAttachEventType } = require('./content/utils');
const { window } = require('./addon/window');
const { getParentWindow } = require('./window/utils');
const { create: makeFrame, getDocShell } = require('./frame/utils');
const { contract } = require('./util/contract');
const { contract: loaderContract } = require('./content/loader');
const { has } = require('./util/array');
const { Rules } = require('./util/rules');
const { merge } = require('./util/object');

const views = WeakMap();
const workers = WeakMap();
const pages = WeakMap();

const readyEventNames = [
  'DOMContentLoaded',
  'document-element-inserted',
  'load'
];

function workerFor(page) workers.get(page)
function pageFor(view) pages.get(view)
function viewFor(page) views.get(page)
function isDisposed (page) !views.get(page, false)

let pageContract = contract(merge({
  allow: {
    is: ['object', 'undefined', 'null'],
    map: function (allow) { return { script: !allow || allow.script !== false }}
  },
  onMessage: {
    is: ['function', 'undefined']
  },
  include: {
    is: ['string', 'array', 'undefined']
  },
  contentScriptWhen: {
    is: ['string', 'undefined']
  }
}, loaderContract.rules));

function enableScript (page) {
  getDocShell(viewFor(page)).allowJavascript = true;
}

function disableScript (page) {
  getDocShell(viewFor(page)).allowJavascript = false;
}

function Allow (page) {
  return {
    get script() getDocShell(viewFor(page)).allowJavascript,
    set script(value) value ? enableScript(page) : disableScript(page)
  };
}

function injectWorker ({page}) {
  let worker = workerFor(page);
  let view = viewFor(page);
  if (isValidURL(page, view.contentDocument.URL))
    attach(worker, view.contentWindow);
}

function isValidURL(page, url) !page.rules || page.rules.matchesAny(url)

const Page = Class({
  implements: [
    EventTarget,
    Disposable
  ],
  extends: WorkerHost(workerFor),
  setup: function Page(options) {
    let page = this;
    options = pageContract(options);
    setListeners(this, options);
    let view = makeFrame(window.document, {
      nodeName: 'iframe',
      type: 'content',
      uri: options.contentURL,
      allowJavascript: options.allow.script,
      allowPlugins: true,
      allowAuth: true
    });

    ['contentScriptFile', 'contentScript', 'contentScriptWhen']
      .forEach(function (prop) page[prop] = options[prop]);

    views.set(this, view);
    pages.set(view, this);

    let worker = new Worker(options);
    workers.set(this, worker);
    pipe(worker, this);

    if (this.include || options.include) {
      this.rules = Rules();
      this.rules.add.apply(this.rules, [].concat(this.include || options.include));
    }
  },
  get allow() Allow(this),
  set allow(value) {
    let allowJavascript = pageContract({ allow: value }).allow.script;
    return allowJavascript ? enableScript(this) : disableScript(this);
  },
  get contentURL() { return viewFor(this).getAttribute('src'); },
  set contentURL(value) {
    if (!isValidURL(this, value)) return;
    let view = viewFor(this);
    let contentURL = pageContract({ contentURL: value }).contentURL;
    view.setAttribute('src', contentURL);
  },
  dispose: function () {
    if (isDisposed(this)) return;
    let view = viewFor(this);
    if (view.parentNode) view.parentNode.removeChild(view);
    views.delete(this);
    detach(workers.get(this));
  },
  toString: function () '[object Page]'
});

exports.Page = Page;

let pageEvents = streamMerge([events, streamEventsFrom(window)]);
let readyEvents = filter(pageEvents, isReadyEvent);
let formattedEvents = map(readyEvents, function({target, type}) {
  return { type: type, page: pageFromDoc(target) };
});
let pageReadyEvents = filter(formattedEvents, function({page, type}) {
  return getAttachEventType(page) === type});
on(pageReadyEvents, 'data', injectWorker);

function isReadyEvent ({type}) {
  return has(readyEventNames, type);
}

/*
 * Takes a document, finds its doc shell tree root and returns the
 * matching Page instance if found
 */
function pageFromDoc(doc) {
  let parentWindow = getParentWindow(doc.defaultView), page;
  if (!parentWindow) return;

  let frames = parentWindow.document.getElementsByTagName('iframe');
  for (let i = frames.length; i--;)
    if (frames[i].contentDocument === doc && (page = pageFor(frames[i])))
      return page;
  return null;
}
