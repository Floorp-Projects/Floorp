/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '> 28'
  }
};

const { Class } = require('../../core/heritage');
const { merge } = require('../../util/object');
const { Disposable } = require('../../core/disposable');
const { on, off, emit, setListeners } = require('../../event/core');
const { EventTarget } = require('../../event/target');
const { getNodeView } = require('../../view/core');

const view = require('./view');
const { toggleButtonContract, toggleStateContract } = require('./contract');
const { properties, render, state, register, unregister,
  setStateFor, getStateFor, getDerivedStateFor } = require('../state');
const { events: stateEvents } = require('../state/events');
const { events: viewEvents } = require('./view/events');
const events = require('../../event/utils');

const { getActiveTab } = require('../../tabs/utils');

const { id: addonID } = require('../../self');
const { identify } = require('../id');

const buttons = new Map();

const toWidgetId = id =>
  ('toggle-button--' + addonID.toLowerCase()+ '-' + id).
    replace(/[^a-z0-9_-]/g, '');

const ToggleButton = Class({
  extends: EventTarget,
  implements: [
    properties(toggleStateContract),
    state(toggleStateContract),
    Disposable
  ],
  setup: function setup(options) {
    let state = merge({
      disabled: false,
      checked: false
    }, toggleButtonContract(options));

    let id = toWidgetId(options.id);

    register(this, state);

    // Setup listeners.
    setListeners(this, options);

    buttons.set(id, this);

    view.create(merge({ type: 'checkbox' }, state, { id: id }));
  },

  dispose: function dispose() {
    let id = toWidgetId(this.id);
    buttons.delete(id);

    off(this);

    view.dispose(id);

    unregister(this);
  },

  get id() {
    return this.state().id;
  },

  click: function click() {
    return view.click(toWidgetId(this.id));
  }
});
exports.ToggleButton = ToggleButton;

identify.define(ToggleButton, ({id}) => toWidgetId(id));

getNodeView.define(ToggleButton, button =>
  view.nodeFor(toWidgetId(button.id))
);

var toggleButtonStateEvents = events.filter(stateEvents,
  e => e.target instanceof ToggleButton);

var toggleButtonViewEvents = events.filter(viewEvents,
  e => buttons.has(e.target));

var clickEvents = events.filter(toggleButtonViewEvents, e => e.type === 'click');
var updateEvents = events.filter(toggleButtonViewEvents, e => e.type === 'update');

on(toggleButtonStateEvents, 'data', ({target, window, state}) => {
  let id = toWidgetId(target.id);

  view.setIcon(id, window, state.icon);
  view.setLabel(id, window, state.label);
  view.setDisabled(id, window, state.disabled);
  view.setChecked(id, window, state.checked);
  view.setBadge(id, window, state.badge, state.badgeColor);
});

on(clickEvents, 'data', ({target: id, window, checked }) => {
  let button = buttons.get(id);
  let windowState = getStateFor(button, window);

  let newWindowState = merge({}, windowState, { checked: checked });

  setStateFor(button, window, newWindowState);

  let state = getDerivedStateFor(button, getActiveTab(window));

  emit(button, 'click', state);

  emit(button, 'change', state);
});

on(updateEvents, 'data', ({target: id, window}) => {
  render(buttons.get(id), window);
});
