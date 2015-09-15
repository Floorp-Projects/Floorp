/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const { contract: loaderContract } = require('./content/loader');
const { contract } = require('./util/contract');
const { WorkerHost, connect } = require('./content/utils');
const { Class } = require('./core/heritage');
const { Disposable } = require('./core/disposable');
const { Worker } = require('./content/worker');
const { EventTarget } = require('./event/target');
const { on, emit, once, setListeners } = require('./event/core');
const { isRegExp, isUndefined } = require('./lang/type');
const { merge, omit } = require('./util/object');
const { remove, has, hasAny } = require("./util/array");
const { Rules } = require("./util/rules");
const { processes, frames, remoteRequire } = require('./remote/parent');
remoteRequire('sdk/content/page-mod');

const pagemods = new Map();
const workers = new Map();
const models = new WeakMap();
var modelFor = (mod) => models.get(mod);
var workerFor = (mod) => workers.get(mod)[0];

// Helper functions
var isRegExpOrString = (v) => isRegExp(v) || typeof v === 'string';

var PAGEMOD_ID = 0;

// Validation Contracts
const modOptions = {
  // contentStyle* / contentScript* are sharing the same validation constraints,
  // so they can be mostly reused, except for the messages.
  contentStyle: merge(Object.create(loaderContract.rules.contentScript), {
    msg: 'The `contentStyle` option must be a string or an array of strings.'
  }),
  contentStyleFile: merge(Object.create(loaderContract.rules.contentScriptFile), {
    msg: 'The `contentStyleFile` option must be a local URL or an array of URLs'
  }),
  include: {
    is: ['string', 'array', 'regexp'],
    ok: (rule) => {
      if (isRegExpOrString(rule))
        return true;
      if (Array.isArray(rule) && rule.length > 0)
        return rule.every(isRegExpOrString);
      return false;
    },
    msg: 'The `include` option must always contain atleast one rule as a string, regular expression, or an array of strings and regular expressions.'
  },
  exclude: {
    is: ['string', 'array', 'regexp', 'undefined'],
    ok: (rule) => {
      if (isRegExpOrString(rule) || isUndefined(rule))
        return true;
      if (Array.isArray(rule) && rule.length > 0)
        return rule.every(isRegExpOrString);
      return false;
    },
    msg: 'If set, the `exclude` option must always contain at least one ' +
      'rule as a string, regular expression, or an array of strings and ' +
      'regular expressions.'
  },
  attachTo: {
    is: ['string', 'array', 'undefined'],
    map: function (attachTo) {
      if (!attachTo) return ['top', 'frame'];
      if (typeof attachTo === 'string') return [attachTo];
      return attachTo;
    },
    ok: function (attachTo) {
      return hasAny(attachTo, ['top', 'frame']) &&
        attachTo.every(has.bind(null, ['top', 'frame', 'existing']));
    },
    msg: 'The `attachTo` option must be a string or an array of strings. ' +
      'The only valid options are "existing", "top" and "frame", and must ' +
      'contain at least "top" or "frame" values.'
  },
};

const modContract = contract(merge({}, loaderContract.rules, modOptions));

/**
 * PageMod constructor (exported below).
 * @constructor
 */
const PageMod = Class({
  implements: [
    modContract.properties(modelFor),
    EventTarget,
    Disposable,
  ],
  extends: WorkerHost(workerFor),
  setup: function PageMod(options) {
    let mod = this;
    let model = modContract(options);
    models.set(this, model);
    model.id = PAGEMOD_ID++;

    let include = model.include;
    model.include = Rules();
    model.include.add.apply(model.include, [].concat(include));

    let exclude = isUndefined(model.exclude) ? [] : model.exclude;
    model.exclude = Rules();
    model.exclude.add.apply(model.exclude, [].concat(exclude));

    // Set listeners on {PageMod} itself, not the underlying worker,
    // like `onMessage`, as it'll get piped.
    setListeners(this, options);

    pagemods.set(model.id, this);
    workers.set(this, []);

    function serializeRules(rules) {
      for (let rule of rules) {
        yield isRegExp(rule) ? { type: "regexp", pattern: rule.source, flags: rule.flags }
                             : { type: "string", value: rule };
      }
    }

    model.childOptions = omit(model, ["include", "exclude", "contentScriptOptions"]);
    model.childOptions.include = [...serializeRules(model.include)];
    model.childOptions.exclude = [...serializeRules(model.exclude)];
    model.childOptions.contentScriptOptions = model.contentScriptOptions ?
                                              JSON.stringify(model.contentScriptOptions) :
                                              null;

    processes.port.emit('sdk/page-mod/create', model.childOptions);
  },

  dispose: function(reason) {
    processes.port.emit('sdk/page-mod/destroy', modelFor(this).id);
    pagemods.delete(modelFor(this).id);
    workers.delete(this);
  },

  destroy: function(reason) {
    // Explicit destroy call, i.e. not via unload so destroy the workers
    let list = workers.get(this);
    if (!list)
      return;

    // Triggers dispose which will cause the child page-mod to be destroyed
    Disposable.prototype.destroy.call(this, reason);

    // Destroy any active workers
    for (let worker of list)
      worker.destroy(reason);
  }
});
exports.PageMod = PageMod;

// Whenever a new process starts send over the list of page-mods
processes.forEvery(process => {
  for (let mod of pagemods.values())
    process.port.emit('sdk/page-mod/create', modelFor(mod).childOptions);
});

frames.port.on('sdk/page-mod/worker-create', (frame, modId, workerOptions) => {
  let mod = pagemods.get(modId);
  if (!mod)
    return;

  // Attach the parent side of the worker to the child
  let worker = Worker();

  workers.get(mod).unshift(worker);
  worker.on('*', (event, ...args) => {
    // page-mod's "attach" event needs to be passed a worker
    if (event === 'attach')
      emit(mod, event, worker)
    else
      emit(mod, event, ...args);
  });

  worker.on('detach', () => {
    let array = workers.get(mod);
    if (array)
      remove(array, worker);
  });

  connect(worker, frame, workerOptions);
});
