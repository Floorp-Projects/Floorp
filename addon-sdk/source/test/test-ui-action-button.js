/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '> 28'
  }
};

const { Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const { data } = require('sdk/self');
const { open, focus, close } = require('sdk/window/helpers');
const { setTimeout } = require('sdk/timers');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { partial } = require('sdk/lang/functional');
const { wait } = require('./event/helpers');
const { gc } = require('sdk/test/memory');
const { emit, once } = require("sdk/event/core");

const openBrowserWindow = partial(open, null, {features: {toolbar: true}});
const openPrivateBrowserWindow = partial(open, null,
  {features: {toolbar: true, private: true}});

const badgeNodeFor = (node) =>
  node.ownerDocument.getAnonymousElementByAttribute(node,
                                      'class', 'toolbarbutton-badge');

function getWidget(buttonId, window = getMostRecentBrowserWindow()) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  const { AREA_NAVBAR } = CustomizableUI;

  let widgets = CustomizableUI.getWidgetIdsInArea(AREA_NAVBAR).
    filter((id) => id.startsWith('action-button--') && id.endsWith(buttonId));

  if (widgets.length === 0)
    throw new Error('Widget with id `' + id +'` not found.');

  if (widgets.length > 1)
    throw new Error('Unexpected number of widgets: ' + widgets.length)

  return CustomizableUI.getWidget(widgets[0]).forWindow(window);
};

exports['test basic constructor validation'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  assert.throws(
    () => ActionButton({}),
    /^The option/,
    'throws on no option given');

  // Test no label
  assert.throws(
    () => ActionButton({ id: 'my-button', icon: './icon.png'}),
    /^The option "label"/,
    'throws on no label given');

  // Test no id
  assert.throws(
    () => ActionButton({ label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no id given');

  // Test no icon
  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'my button' }),
    /^The option "icon"/,
    'throws on no icon given');


  // Test empty label
  assert.throws(
    () => ActionButton({ id: 'my-button', label: '', icon: './icon.png' }),
    /^The option "label"/,
    'throws on no valid label given');

  // Test invalid id
  assert.throws(
    () => ActionButton({ id: 'my button', label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no valid id given');

  // Test empty id
  assert.throws(
    () => ActionButton({ id: '', label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no valid id given');

  // Test remote icon
  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'my button', icon: 'http://www.mozilla.org/favicon.ico'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: no absolute URI to local resource, neither relative './'
  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'my button', icon: 'icon.png'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: no absolute URI to local resource, neither relative './'
  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'my button', icon: 'foo and bar'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: '../' is not allowed
  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'my button', icon: '../icon.png'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'button', icon: './i.png', badge: true}),
    /^The option "badge"/,
    'throws on no valid badge given');

  assert.throws(
    () => ActionButton({ id: 'my-button', label: 'button', icon: './i.png', badgeColor: true}),
    /^The option "badgeColor"/,
    'throws on no valid badge given');

  loader.unload();
};

exports['test button added'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  let button = ActionButton({
    id: 'my-button-1',
    label: 'my button',
    icon: './icon.png'
  });

  // check defaults
  assert.equal(button.disabled, false,
    'disabled is set to default `false` value');

  let { node } = getWidget(button.id);

  assert.ok(!!node, 'The button is in the navbar');

  assert.equal(button.label, node.getAttribute('label'),
    'label is set');

  assert.equal(button.label, node.getAttribute('tooltiptext'),
    'tooltip is set');

  assert.equal(data.url(button.icon.substr(2)), node.getAttribute('image'),
    'icon is set');

  assert.equal("", node.getAttribute('badge'),
    'badge attribute is empty');

  loader.unload();
}

exports['test button is not garbaged'] = function (assert, done) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  ActionButton({
    id: 'my-button-1',
    label: 'my button',
    icon: './icon.png',
    onClick: () => {
      loader.unload();
      done();
    }
  });

  gc().then(() => {
    let { node } = getWidget('my-button-1');

    assert.ok(!!node, 'The button is in the navbar');

    assert.equal('my button', node.getAttribute('label'),
      'label is set');

    assert.equal(data.url('icon.png'), node.getAttribute('image'),
      'icon is set');

    // ensure the listener is not gc'ed too
    node.click();
  }).catch(assert.fail);
}

exports['test button added with resource URI'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  let button = ActionButton({
    id: 'my-button-1',
    label: 'my button',
    icon: data.url('icon.png')
  });

  assert.equal(button.icon, data.url('icon.png'),
    'icon is set');

  let { node } = getWidget(button.id);

  assert.equal(button.icon, node.getAttribute('image'),
    'icon on node is set');

  loader.unload();
}

exports['test button duplicate id'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let button = ActionButton({
    id: 'my-button-2',
    label: 'my button',
    icon: './icon.png'
  });

  assert.throws(() => {
    let doppelganger = ActionButton({
      id: 'my-button-2',
      label: 'my button',
      icon: './icon.png'
    });
  },
  /^The ID/,
  'No duplicates allowed');

  loader.unload();
}

exports['test button multiple destroy'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let button = ActionButton({
    id: 'my-button-2',
    label: 'my button',
    icon: './icon.png'
  });

  button.destroy();
  button.destroy();
  button.destroy();

  assert.pass('multiple destroy doesn\'t matter');

  loader.unload();
}

exports['test button removed on dispose'] = function(assert, done) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let widgetId;

  CustomizableUI.addListener({
    onWidgetDestroyed: function(id) {
      if (id === widgetId) {
        CustomizableUI.removeListener(this);

        assert.pass('button properly removed');
        loader.unload();
        done();
      }
    }
  });

  let button = ActionButton({
    id: 'my-button-3',
    label: 'my button',
    icon: './icon.png'
  });

  // Tried to use `getWidgetIdsInArea` but seems undefined, not sure if it
  // was removed or it's not in the UX build yet
  widgetId = getWidget(button.id).id;

  button.destroy();
};

exports['test button global state updated'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require("sdk/self");

  let button = ActionButton({
    id: 'my-button-4',
    label: 'my button',
    icon: './icon.png',
  });
  assert.pass('the button was created.');

  // Tried to use `getWidgetIdsInArea` but seems undefined, not sure if it
  // was removed or it's not in the UX build yet

  let { node, id: widgetId } = getWidget(button.id);

  // check read-only properties

  assert.throws(() => button.id = 'another-id',
    /^setting getter-only property/,
    'id cannot be set at runtime');

  assert.equal(button.id, 'my-button-4',
    'id is unchanged');
  assert.equal(node.id, widgetId,
    'node id is unchanged');

  // check writable properties

  button.label = 'New label';
  assert.equal(button.label, 'New label',
    'label is updated');
  assert.equal(node.getAttribute('label'), 'New label',
    'node label is updated');
  assert.equal(node.getAttribute('tooltiptext'), 'New label',
    'node tooltip is updated');

  button.icon = './new-icon.png';
  assert.equal(button.icon, './new-icon.png',
    'icon is updated');
  assert.equal(node.getAttribute('image'), data.url('new-icon.png'),
    'node image is updated');

  button.disabled = true;
  assert.equal(button.disabled, true,
    'disabled is updated');
  assert.equal(node.getAttribute('disabled'), 'true',
    'node disabled is updated');

  button.badge = '+2';
  button.badgeColor = 'blue';

  assert.equal(button.badge, '+2',
    'badge is updated');
  assert.equal(node.getAttribute('bagde'), '',
    'node badge is updated');

  assert.equal(button.badgeColor, 'blue',
    'badgeColor is updated');
  assert.equal(badgeNodeFor(node).style.backgroundColor, 'blue',
    'badge color is updated');

  // TODO: test validation on update

  loader.unload();
}

exports['test button global state set and get with state method'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let button = ActionButton({
    id: 'my-button-16',
    label: 'my button',
    icon: './icon.png'
  });

  // read the button's state
  let state = button.state(button);

  assert.equal(state.label, 'my button',
    'label is correct');
  assert.equal(state.icon, './icon.png',
    'icon is correct');
  assert.equal(state.disabled, false,
    'disabled is correct');

  // set the new button's state
  button.state(button, {
    label: 'New label',
    icon: './new-icon.png',
    disabled: true,
    badge: '+2',
    badgeColor: 'blue'
  });

  assert.equal(button.label, 'New label',
    'label is updated');
  assert.equal(button.icon, './new-icon.png',
    'icon is updated');
  assert.equal(button.disabled, true,
    'disabled is updated');
  assert.equal(button.badge, '+2',
    'badge is updated');
  assert.equal(button.badgeColor, 'blue',
    'badgeColor is updated');

  loader.unload();
}

exports['test button global state updated on multiple windows'] = function*(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  let button = ActionButton({
    id: 'my-button-5',
    label: 'my button',
    icon: './icon.png'
  });
  assert.pass('the button was created');

  let nodes = [ getWidget(button.id).node ];

  let window = yield openBrowserWindow();
  assert.pass('the window was created');

  nodes.push(getWidget(button.id, window).node);

  button.label = 'New label';
  button.icon = './new-icon.png';
  button.disabled = true;
  button.badge = '+10';
  button.badgeColor = 'green';

  for (let node of nodes) {
    assert.equal(node.getAttribute('label'), 'New label',
      'node label is updated');
    assert.equal(node.getAttribute('tooltiptext'), 'New label',
      'node tooltip is updated');

    assert.equal(button.icon, './new-icon.png',
      'icon is updated');
    assert.equal(node.getAttribute('image'), data.url('new-icon.png'),
      'node image is updated');

    assert.equal(button.disabled, true,
      'disabled is updated');
    assert.equal(node.getAttribute('disabled'), 'true',
      'node disabled is updated');

    assert.equal(button.badge, '+10',
      'badge is updated')
    assert.equal(button.badgeColor, 'green',
      'badgeColor is updated')
    assert.equal(node.getAttribute('badge'), '+10',
      'node badge is updated')
    assert.equal(badgeNodeFor(node).style.backgroundColor, 'green',
      'node badge color is updated')
  };

  yield close(window);

  loader.unload();
};

exports['test button window state'] = function*(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let { data } = loader.require('sdk/self');

  let button = ActionButton({
    id: 'my-button-6',
    label: 'my button',
    icon: './icon.png',
    badge: '+1',
    badgeColor: 'red'
  });

  let mainWindow = browserWindows.activeWindow;
  let nodes = [getWidget(button.id).node];

  let window = yield openBrowserWindow().then(focus);

  nodes.push(getWidget(button.id, window).node);

  let { activeWindow } = browserWindows;

  button.state(activeWindow, {
    label: 'New label',
    icon: './new-icon.png',
    disabled: true,
    badge: '+2',
    badgeColor : 'green'
  });

  // check the states

  assert.equal(button.label, 'my button',
    'global label unchanged');
  assert.equal(button.icon, './icon.png',
    'global icon unchanged');
  assert.equal(button.disabled, false,
    'global disabled unchanged');
  assert.equal(button.badge, '+1',
    'global badge unchanged');
  assert.equal(button.badgeColor, 'red',
    'global badgeColor unchanged');

  let state = button.state(mainWindow);

  assert.equal(state.label, 'my button',
    'previous window label unchanged');
  assert.equal(state.icon, './icon.png',
    'previous window icon unchanged');
  assert.equal(state.disabled, false,
    'previous window disabled unchanged');
  assert.deepEqual(button.badge, '+1',
    'previouswindow badge unchanged');
  assert.deepEqual(button.badgeColor, 'red',
    'previous window badgeColor unchanged');

  state = button.state(activeWindow);

  assert.equal(state.label, 'New label',
    'active window label updated');
  assert.equal(state.icon, './new-icon.png',
    'active window icon updated');
  assert.equal(state.disabled, true,
    'active disabled updated');
  assert.equal(state.badge, '+2',
    'active badge updated');
  assert.equal(state.badgeColor, 'green',
    'active badgeColor updated');

  // change the global state, only the windows without a state are affected

  button.label = 'A good label';
  button.badge = '+3';

  assert.equal(button.label, 'A good label',
    'global label updated');
  assert.equal(button.state(mainWindow).label, 'A good label',
    'previous window label updated');
  assert.equal(button.state(activeWindow).label, 'New label',
    'active window label unchanged');
  assert.equal(button.state(activeWindow).badge, '+2',
    'active badge unchanged');
  assert.equal(button.state(activeWindow).badgeColor, 'green',
    'active badgeColor unchanged');
  assert.equal(button.state(mainWindow).badge, '+3',
    'previous window badge updated');
  assert.equal(button.state(mainWindow).badgeColor, 'red',
    'previous window badgeColor unchanged');

  // delete the window state will inherits the global state again

  button.state(activeWindow, null);

  state = button.state(activeWindow);

  assert.equal(state.label, 'A good label',
    'active window label inherited');
  assert.equal(state.badge, '+3',
    'previous window badge inherited');
  assert.equal(button.badgeColor, 'red',
    'previous window badgeColor inherited');

  // check the nodes properties
  let node = nodes[0];

  state = button.state(mainWindow);

  assert.equal(node.getAttribute('label'), state.label,
    'node label is correct');
  assert.equal(node.getAttribute('tooltiptext'), state.label,
    'node tooltip is correct');

  assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
    'node image is correct');
  assert.equal(node.hasAttribute('disabled'), state.disabled,
    'disabled is correct');
  assert.equal(node.getAttribute("badge"), state.badge,
    'badge is correct');

  assert.equal(badgeNodeFor(node).style.backgroundColor, state.badgeColor,
    'badge color is correct');

  node = nodes[1];
  state = button.state(activeWindow);

  assert.equal(node.getAttribute('label'), state.label,
    'node label is correct');
  assert.equal(node.getAttribute('tooltiptext'), state.label,
    'node tooltip is correct');

  assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
    'node image is correct');
  assert.equal(node.hasAttribute('disabled'), state.disabled,
    'disabled is correct');
  assert.equal(node.getAttribute('badge'), state.badge,
    'badge is correct');

  assert.equal(badgeNodeFor(node).style.backgroundColor, state.badgeColor,
    'badge color is correct');

  yield close(window);

  loader.unload();
};


exports['test button tab state'] = function*(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let { data } = loader.require('sdk/self');
  let tabs = loader.require('sdk/tabs');

  let button = ActionButton({
    id: 'my-button-7',
    label: 'my button',
    icon: './icon.png'
  });

  let mainTab = tabs.activeTab;
  let node = getWidget(button.id).node;

  tabs.open('about:blank');

  yield wait(tabs, 'ready');

  let tab = tabs.activeTab;
  let { activeWindow } = browserWindows;

  // set window state
  button.state(activeWindow, {
    label: 'Window label',
    icon: './window-icon.png',
    badge: 'win',
    badgeColor: 'blue'
  });

  // set previous active tab state
  button.state(mainTab, {
    label: 'Tab label',
    icon: './tab-icon.png',
    badge: 'tab',
    badgeColor: 'red'
  });

  // set current active tab state
  button.state(tab, {
    icon: './another-tab-icon.png',
    disabled: true,
    badge: 't1',
    badgeColor: 'green'
  });

  // check the states, be sure they won't be gc'ed
  yield gc();

  assert.equal(button.label, 'my button',
    'global label unchanged');
  assert.equal(button.icon, './icon.png',
    'global icon unchanged');
  assert.equal(button.disabled, false,
    'global disabled unchanged');
  assert.equal(button.badge, undefined,
    'global badge unchanged')

  let state = button.state(mainTab);

  assert.equal(state.label, 'Tab label',
    'previous tab label updated');
  assert.equal(state.icon, './tab-icon.png',
    'previous tab icon updated');
  assert.equal(state.disabled, false,
    'previous tab disabled unchanged');
  assert.equal(state.badge, 'tab',
    'previous tab badge unchanged')
  assert.equal(state.badgeColor, 'red',
    'previous tab badgeColor unchanged')

  state = button.state(tab);

  assert.equal(state.label, 'Window label',
    'active tab inherited from window state');
  assert.equal(state.icon, './another-tab-icon.png',
    'active tab icon updated');
  assert.equal(state.disabled, true,
    'active disabled updated');
  assert.equal(state.badge, 't1',
    'active badge updated');
  assert.equal(state.badgeColor, 'green',
    'active badgeColor updated');

  // change the global state
  button.icon = './good-icon.png';

  // delete the tab state
  button.state(tab, null);

  assert.equal(button.icon, './good-icon.png',
    'global icon updated');
  assert.equal(button.state(mainTab).icon, './tab-icon.png',
    'previous tab icon unchanged');
  assert.equal(button.state(tab).icon, './window-icon.png',
    'tab icon inherited from window');
  assert.equal(button.state(mainTab).badge, 'tab',
    'previous tab badge is unchaged');
  assert.equal(button.state(tab).badge, 'win',
    'tab badge is inherited from window');

  // delete the window state
  button.state(activeWindow, null);

  state = button.state(tab);

  assert.equal(state.icon, './good-icon.png',
    'tab icon inherited from global');
  assert.equal(state.badge, undefined,
    'tab badge inherited from global');
  assert.equal(state.badgeColor, undefined,
    'tab badgeColor inherited from global');

  // check the node properties
  yield new Promise(resolve => {
    let target = {};
    once(target, "ready", resolve);
    emit(target, "ready");
  });

  assert.equal(node.getAttribute('label'), state.label,
    'node label is correct');
  assert.equal(node.getAttribute('tooltiptext'), state.label,
    'node tooltip is correct');
  assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
    'node image is correct');
  assert.equal(node.hasAttribute('disabled'), state.disabled,
    'node disabled is correct');
  assert.equal(node.getAttribute('badge'), '',
    'badge text is correct');
  assert.equal(badgeNodeFor(node).style.backgroundColor, '',
    'badge color is correct');

  mainTab.activate();

  yield wait(tabs, 'activate');

  // This is made in order to avoid to check the node before it
  // is updated, need a better check
  yield new Promise(resolve => {
    let target = {};
    once(target, "ready", resolve);
    emit(target, "ready");
  });

  state = button.state(mainTab);

  assert.equal(node.getAttribute('label'), state.label,
    'node label is correct');
  assert.equal(node.getAttribute('tooltiptext'), state.label,
    'node tooltip is correct');
  assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
    'node image is correct');
  assert.equal(node.hasAttribute('disabled'), state.disabled,
    'disabled is correct');
  assert.equal(node.getAttribute('badge'), state.badge,
    'badge text is correct');
  assert.equal(badgeNodeFor(node).style.backgroundColor, state.badgeColor,
    'badge color is correct');

  tab.close(loader.unload);

  loader.unload();
};

exports['test button click'] = function*(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let labels = [];

  let button = ActionButton({
    id: 'my-button-8',
    label: 'my button',
    icon: './icon.png',
    onClick: ({label}) => labels.push(label)
  });

  let mainWindow = browserWindows.activeWindow;
  let chromeWindow = getMostRecentBrowserWindow();

  let window = yield openBrowserWindow().then(focus);

  button.state(mainWindow, { label: 'nothing' });
  button.state(mainWindow.tabs.activeTab, { label: 'foo'})
  button.state(browserWindows.activeWindow, { label: 'bar' });

  button.click();

  yield focus(chromeWindow);

  button.click();

  assert.deepEqual(labels, ['bar', 'foo'],
    'button click works');

  yield close(window);

  loader.unload();
}

exports['test button icon set'] = function(assert) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  // Test remote icon set
  assert.throws(
    () => ActionButton({
      id: 'my-button-10',
      label: 'my button',
      icon: {
        '16': 'http://www.mozilla.org/favicon.ico'
      }
    }),
    /^The option "icon"/,
    'throws on no valid icon given');

  let button = ActionButton({
    id: 'my-button-11',
    label: 'my button',
    icon: {
      '5': './icon5.png',
      '16': './icon16.png',
      '32': './icon32.png',
      '64': './icon64.png'
    }
  });

  let { node, id: widgetId } = getWidget(button.id);
  let { devicePixelRatio } = node.ownerGlobal;

  let size = 16 * devicePixelRatio;

  assert.equal(node.getAttribute('image'), data.url(button.icon[size].substr(2)),
    'the icon is set properly in navbar');

  size = 32 * devicePixelRatio;

  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_PANEL);

  assert.equal(node.getAttribute('image'), data.url(button.icon[size].substr(2)),
    'the icon is set properly in panel');

  // Using `loader.unload` without move back the button to the original area
  // raises an error in the CustomizableUI. This is doesn't happen if the
  // button is moved manually from navbar to panel. I believe it has to do
  // with `addWidgetToArea` method, because even with a `timeout` the issue
  // persist.
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);

  loader.unload();
}

exports['test button icon set with only one option'] = function(assert) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { data } = loader.require('sdk/self');

  // Test remote icon set
  assert.throws(
    () => ActionButton({
      id: 'my-button-10',
      label: 'my button',
      icon: {
        '16': 'http://www.mozilla.org/favicon.ico'
      }
    }),
    /^The option "icon"/,
    'throws on no valid icon given');

  let button = ActionButton({
    id: 'my-button-11',
    label: 'my button',
    icon: {
      '5': './icon5.png'
    }
  });

  let { node, id: widgetId } = getWidget(button.id);

  assert.equal(node.getAttribute('image'), data.url(button.icon['5'].substr(2)),
    'the icon is set properly in navbar');

  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_PANEL);

  assert.equal(node.getAttribute('image'), data.url(button.icon['5'].substr(2)),
    'the icon is set properly in panel');

  // Using `loader.unload` without move back the button to the original area
  // raises an error in the CustomizableUI. This is doesn't happen if the
  // button is moved manually from navbar to panel. I believe it has to do
  // with `addWidgetToArea` method, because even with a `timeout` the issue
  // persist.
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);

  loader.unload();
}

exports['test button state validation'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ActionButton({
    id: 'my-button-12',
    label: 'my button',
    icon: './icon.png'
  })

  let state = button.state(button);

  assert.throws(
    () => button.state(button, { icon: 'http://www.mozilla.org/favicon.ico' }),
    /^The option "icon"/,
    'throws on remote icon given');

  assert.throws(
    () => button.state(button, { badge: true } ),
    /^The option "badge"/,
    'throws on wrong badge value given');

  loader.unload();
};

exports['test button are not in private windows'] = function(assert, done) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let{ isPrivate } = loader.require('sdk/private-browsing');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ActionButton({
    id: 'my-button-13',
    label: 'my button',
    icon: './icon.png'
  });

  openPrivateBrowserWindow().then(window => {
    assert.ok(isPrivate(window),
      'the new window is private');

    let { node } = getWidget(button.id, window);

    assert.ok(!node || node.style.display === 'none',
      'the button is not added / is not visible on private window');

    return window;
  }).
  then(close).
  then(loader.unload).
  then(done, assert.fail)
}

exports['test button state are snapshot'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let tabs = loader.require('sdk/tabs');

  let button = ActionButton({
    id: 'my-button-14',
    label: 'my button',
    icon: './icon.png'
  });

  let state = button.state(button);
  let windowState = button.state(browserWindows.activeWindow);
  let tabState = button.state(tabs.activeTab);

  assert.deepEqual(windowState, state,
    'window state has the same properties of button state');

  assert.deepEqual(tabState, state,
    'tab state has the same properties of button state');

  assert.notEqual(windowState, state,
    'window state is not the same object of button state');

  assert.notEqual(tabState, state,
    'tab state is not the same object of button state');

  assert.deepEqual(button.state(button), state,
    'button state has the same content of previous button state');

  assert.deepEqual(button.state(browserWindows.activeWindow), windowState,
    'window state has the same content of previous window state');

  assert.deepEqual(button.state(tabs.activeTab), tabState,
    'tab state has the same content of previous tab state');

  assert.notEqual(button.state(button), state,
    'button state is not the same object of previous button state');

  assert.notEqual(button.state(browserWindows.activeWindow), windowState,
    'window state is not the same object of previous window state');

  assert.notEqual(button.state(tabs.activeTab), tabState,
    'tab state is not the same object of previous tab state');

  loader.unload();
}

exports['test button icon object is a snapshot'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let icon = {
    '16': './foo.png'
  };

  let button = ActionButton({
    id: 'my-button-17',
    label: 'my button',
    icon: icon
  });

  assert.deepEqual(button.icon, icon,
    'button.icon has the same properties of the object set in the constructor');

  assert.notEqual(button.icon, icon,
    'button.icon is not the same object of the object set in the constructor');

  assert.throws(
    () => button.icon[16] = './bar.png',
    /16 is read-only/,
    'properties of button.icon are ready-only'
  );

  let newIcon = {'16': './bar.png'};
  button.icon = newIcon;

  assert.deepEqual(button.icon, newIcon,
    'button.icon has the same properties of the object set');

  assert.notEqual(button.icon, newIcon,
    'button.icon is not the same object of the object set');

  loader.unload();
}

exports['test button after destroy'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let { activeTab } = loader.require('sdk/tabs');

  let button = ActionButton({
    id: 'my-button-15',
    label: 'my button',
    icon: './icon.png',
    onClick: () => assert.fail('onClick should not be called')
  });

  button.destroy();

  assert.throws(
    () => button.click(),
    /^The state cannot be set or get/,
    'button.click() not executed');

  assert.throws(
    () => button.label,
    /^The state cannot be set or get/,
    'button.label cannot be get after destroy');

  assert.throws(
    () => button.label = 'my label',
    /^The state cannot be set or get/,
    'button.label cannot be set after destroy');

  assert.throws(
    () => {
      button.state(browserWindows.activeWindow, {
        label: 'window label'
      });
    },
    /^The state cannot be set or get/,
    'window state label cannot be set after destroy');

  assert.throws(
    () => button.state(browserWindows.activeWindow).label,
    /^The state cannot be set or get/,
    'window state label cannot be get after destroy');

  assert.throws(
    () => {
      button.state(activeTab, {
        label: 'tab label'
      });
    },
    /^The state cannot be set or get/,
    'tab state label cannot be set after destroy');

  assert.throws(
    () => button.state(activeTab).label,
    /^The state cannot be set or get/,
    'window state label cannot se get after destroy');

  loader.unload();
};

exports['test button badge property'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let button = ActionButton({
    id: 'my-button-18',
    label: 'my button',
    icon: './icon.png',
    badge: 123456
  });

  assert.equal(button.badge, 123456,
    'badge is set');

  assert.equal(button.badgeColor, undefined,
    'badge color is not set');

  let { node } = getWidget(button.id);
  let { getComputedStyle } = node.ownerGlobal;
  let badgeNode = badgeNodeFor(node);

  assert.equal('1234', node.getAttribute('badge'),
    'badge text is displayed up to four characters');

  assert.equal(getComputedStyle(badgeNode).backgroundColor, 'rgb(217, 0, 0)',
    'badge color is the default one');

  button.badge = '危機';

  assert.equal(button.badge, '危機',
    'badge is properly set');

  assert.equal('危機', node.getAttribute('badge'),
    'badge text is displayed');

  button.badge = '🐶🐰🐹';

  assert.equal(button.badge, '🐶🐰🐹',
    'badge is properly set');

  assert.equal('🐶🐰🐹', node.getAttribute('badge'),
    'badge text is displayed');

  loader.unload();
}
exports['test button badge color'] = function(assert) {
  let loader = Loader(module);
  let { ActionButton } = loader.require('sdk/ui');

  let button = ActionButton({
    id: 'my-button-19',
    label: 'my button',
    icon: './icon.png',
    badge: '+1',
    badgeColor: 'blue'
  });

  assert.equal(button.badgeColor, 'blue',
    'badge color is set');

  let { node } = getWidget(button.id);
  let { getComputedStyle } = node.ownerGlobal;
  let badgeNode = badgeNodeFor(node);

  assert.equal(badgeNodeFor(node).style.backgroundColor, 'blue',
    'badge color is displayed properly');
  assert.equal(getComputedStyle(badgeNode).backgroundColor, 'rgb(0, 0, 255)',
    'badge color overrides the default one');

  loader.unload();
}

require("sdk/test").run(module.exports);
