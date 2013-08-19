/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '> 24'
  }
};

try {
  require('chrome').Cu.import('resource:///modules/CustomizableUI.jsm', {});
}
catch (e) {
  throw Error('Unsupported Application: The module ' + module.id + ' does not support this application.');
}

const { Class } = require('../core/heritage');
const { merge } = require('../util/object');
const { properties, render, state, register, unregister } = require("./state");
const { Disposable } = require('../core/disposable');
const { contract } = require('../util/contract');
const { on, off, emit, setListeners } = require('../event/core');
const { EventTarget } = require('../event/target');

const { isNil, isObject, isString } = require('../lang/type');
const { required, either, string, number, boolean, object } = require('../deprecated/api-utils');
const { isLocalURL } = require('../url');

const { add, remove, has, clear, iterator } = require("../lang/weak-set");

const tabs = require("../tabs");
const { browserWindows } = require("sdk/windows");

const view = require("./button/view");

const { data } = require("../self");

function isIconSet(icons) {
  return Object.keys(icons).
    every(size => String(size >>> 0) === size && isLocalURL(icons[size]))
}

let iconSet = {
  is: either(object, string),
  ok: v => (isString(v) && isLocalURL(v)) || (isObject(v) && isIconSet(v)),
  msg: 'The option "icon" must be a local URL or an object with ' +
    'numeric keys / local URL values pair.'
}

let buttonId = {
  is: string,
  ok: v => /^[a-z0-9-_]+$/i.test(v),
  msg: 'The option "id" must be a valid alphanumeric id (hyphens and ' +
        'underscores are allowed).'
};

let buttonType = {
  is: string,
  ok: v => ~['button', 'checkbox'].indexOf(v),
  msg: 'The option "type" must be one of the following string values: ' +
    '"button", "checkbox".'
}

let size = {
  is: string,
  ok: v => ~['small', 'medium', 'large'].indexOf(v),
  msg: 'The option "size" must be one of the following string values: ' +
    '"small", "medium", "large".'
};

let label = {
  is: string,
  ok: v => isNil(v) || v.trim().length > 0,
  msg: 'The option "label" must be a non empty string'
}

let stateContract = contract({
  label: label,
  icon: iconSet,
  disabled: boolean,
  checked: boolean
});

let buttonContract = contract(merge({}, stateContract.rules, {
  id: required(buttonId),
  label: required(label),
  icon: required(iconSet),
  type: buttonType,
  size: size
}));

const Button = Class({
  extends: EventTarget,
  implements: [
    properties(stateContract),
    state(stateContract),
    Disposable
  ],
  setup: function setup(options) {
    let state = merge({
      type: 'button',
      disabled: false,
      checked: false,
      size: 'small',
    }, buttonContract(options));

    // Setup listeners.
    setListeners(this, options);

    // TODO: improve
    let viewEvents = view.create(state);

    on(viewEvents, 'click', onViewClick.bind(this));
    on(viewEvents, 'moved', () => render(this));

    register(this, state);
  },

  dispose: function dispose() {
    off(this);

    view.dispose(this);

    unregister(this);
  },

  get id() this.state().id,
  get size() this.state().size,
  get type() this.state().type,

  click: function click() view.click(this)
});
exports.Button = Button;

function onViewClick() {
  let state = this.state(tabs.activeTab);

  if (this.type === 'checkbox') {
    state = merge({}, state, { checked: !state.checked });

    this.state(browserWindows.activeWindow, state);

    emit(this, 'click', state);

    emit(this, 'change', state);
  }
  else
    emit(this, 'click', state);
}

on(Button, 'render', function(button, window, state) {
  view.setIcon(button, window, state.icon);
  view.setLabel(button, window, state.label);
  view.setDisabled(button, window, state.disabled);
  view.setChecked(button, window, state.checked);
});
