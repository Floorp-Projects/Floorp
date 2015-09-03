/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

let { merge } = require('../util/object');
let { data } = require('../self');
let assetsURI = data.url();
let isArray = Array.isArray;
let method = require('../../method/core');
let { uuid } = require('../util/uuid');

const isAddonContent = ({ contentURL }) =>
  contentURL && data.url(contentURL).startsWith(assetsURI);

exports.isAddonContent = isAddonContent;

function hasContentScript({ contentScript, contentScriptFile }) {
  return (isArray(contentScript) ? contentScript.length > 0 :
         !!contentScript) ||
         (isArray(contentScriptFile) ? contentScriptFile.length > 0 :
         !!contentScriptFile);
}
exports.hasContentScript = hasContentScript;

function requiresAddonGlobal(model) {
  return model.injectInDocument || (isAddonContent(model) && !hasContentScript(model));
}
exports.requiresAddonGlobal = requiresAddonGlobal;

function getAttachEventType(model) {
  if (!model) return null;
  let when = model.contentScriptWhen;
  return requiresAddonGlobal(model) ? 'document-element-inserted' :
         when === 'start' ? 'document-element-inserted' :
         when === 'ready' ? 'DOMContentLoaded' :
         when === 'end' ? 'load' :
         null;
}
exports.getAttachEventType = getAttachEventType;

let attach = method('worker-attach');
exports.attach = attach;

let connect = method('worker-connect');
exports.connect = connect;

let detach = method('worker-detach');
exports.detach = detach;

let destroy = method('worker-destroy');
exports.destroy = destroy;

function WorkerHost (workerFor) {
  // Define worker properties that just proxy to underlying worker
  return ['postMessage', 'port', 'url', 'tab'].reduce(function(proto, name) {
    // Use descriptor properties instead so we can call
    // the worker function in the context of the worker so we
    // don't have to create new functions with `fn.bind(worker)`
    let descriptorProp = {
      value: function (...args) {
        let worker = workerFor(this);
        return worker[name].apply(worker, args);
      }
    };
    
    let accessorProp = {
      get: function () { return workerFor(this)[name]; },
      set: function (value) { workerFor(this)[name] = value; }
    };

    Object.defineProperty(proto, name, merge({
      enumerable: true,
      configurable: false,
    }, isDescriptor(name) ? descriptorProp : accessorProp));
    return proto;
  }, {});
  
  function isDescriptor (prop) {
    return ~['postMessage'].indexOf(prop);
  }
}
exports.WorkerHost = WorkerHost;

function makeChildOptions(options) {
  function makeStringArray(arrayOrValue) {
    if (!arrayOrValue)
      return [];
    return [String(v) for (v of [].concat(arrayOrValue))];
  }

  return {
    id: String(uuid()),
    contentScript: makeStringArray(options.contentScript),
    contentScriptFile: makeStringArray(options.contentScriptFile),
    contentScriptOptions: options.contentScriptOptions ?
                          JSON.stringify(options.contentScriptOptions) :
                          null,
  }
}
exports.makeChildOptions = makeChildOptions;
