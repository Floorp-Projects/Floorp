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

const openBrowserWindow = partial(open, null, {features: {toolbar: true}});
const openPrivateBrowserWindow = partial(open, null,
  {features: {toolbar: true, private: true}});

function getWidget(buttonId, window = getMostRecentBrowserWindow()) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  const { AREA_NAVBAR } = CustomizableUI;

  let widgets = CustomizableUI.getWidgetIdsInArea(AREA_NAVBAR).
    filter((id) => id.startsWith('toggle-button--') && id.endsWith(buttonId));

  if (widgets.length === 0)
    throw new Error('Widget with id `' + id +'` not found.');

  if (widgets.length > 1)
    throw new Error('Unexpected number of widgets: ' + widgets.length)

  return CustomizableUI.getWidget(widgets[0]).forWindow(window);
};

exports['test basic constructor validation'] = function(assert) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  assert.throws(
    () => ToggleButton({}),
    /^The option/,
    'throws on no option given');

  // Test no label
  assert.throws(
    () => ToggleButton({ id: 'my-button', icon: './icon.png'}),
    /^The option "label"/,
    'throws on no label given');

  // Test no id
  assert.throws(
    () => ToggleButton({ label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no id given');

  // Test no icon
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: 'my button' }),
    /^The option "icon"/,
    'throws on no icon given');


  // Test empty label
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: '', icon: './icon.png' }),
    /^The option "label"/,
    'throws on no valid label given');

  // Test invalid id
  assert.throws(
    () => ToggleButton({ id: 'my button', label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no valid id given');

  // Test empty id
  assert.throws(
    () => ToggleButton({ id: '', label: 'my button', icon: './icon.png' }),
    /^The option "id"/,
    'throws on no valid id given');

  // Test remote icon
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: 'my button', icon: 'http://www.mozilla.org/favicon.ico'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: no absolute URI to local resource, neither relative './'
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: 'my button', icon: 'icon.png'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: no absolute URI to local resource, neither relative './'
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: 'my button', icon: 'foo and bar'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong icon: '../' is not allowed
  assert.throws(
    () => ToggleButton({ id: 'my-button', label: 'my button', icon: '../icon.png'}),
    /^The option "icon"/,
    'throws on no valid icon given');

  // Test wrong checked
  assert.throws(
    () => ToggleButton({
      id: 'my-button', label: 'my button', icon: './icon.png', checked: 'yes'}),
    /^The option "checked"/,
    'throws on no valid checked value given');

  loader.unload();
};

exports['test button added'] = function(assert) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
    id: 'my-button-1',
    label: 'my button',
    icon: './icon.png'
  });

  // check defaults
  assert.equal(button.checked, false,
    'checked is set to default `false` value');

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

  loader.unload();
}

exports['test button added with resource URI'] = function(assert) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
    id: 'my-button-2',
    label: 'my button',
    icon: './icon.png'
  });

  assert.throws(() => {
    let doppelganger = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');

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

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
    id: 'my-button-4',
    label: 'my button',
    icon: './icon.png'
  });

  // Tried to use `getWidgetIdsInArea` but seems undefined, not sure if it
  // was removed or it's not in the UX build yet

  let { node, id: widgetId } = getWidget(button.id);

  // check read-only properties

  assert.throws(() => button.id = 'another-id',
    /^setting a property that has only a getter/,
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

  // TODO: test validation on update

  loader.unload();
}

exports['test button global state set and get with state method'] = function(assert) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
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
    disabled: true
  });

  assert.equal(button.label, 'New label',
    'label is updated');
  assert.equal(button.icon, './new-icon.png',
    'icon is updated');
  assert.equal(button.disabled, true,
    'disabled is updated');

  loader.unload();
};

exports['test button global state updated on multiple windows'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  let button = ToggleButton({
    id: 'my-button-5',
    label: 'my button',
    icon: './icon.png'
  });

  let nodes = [getWidget(button.id).node];

  openBrowserWindow().then(window => {
    nodes.push(getWidget(button.id, window).node);

    button.label = 'New label';
    button.icon = './new-icon.png';
    button.disabled = true;

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
    };

    return window;
  }).
  then(close).
  then(loader.unload).
  then(done, assert.fail);
};

exports['test button window state'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ToggleButton({
    id: 'my-button-6',
    label: 'my button',
    icon: './icon.png'
  });

  let mainWindow = browserWindows.activeWindow;
  let nodes = [getWidget(button.id).node];

  openBrowserWindow().then(focus).then(window => {
    nodes.push(getWidget(button.id, window).node);

    let { activeWindow } = browserWindows;

    button.state(activeWindow, {
      label: 'New label',
      icon: './new-icon.png',
      disabled: true
    });

    // check the states

    assert.equal(button.label, 'my button',
      'global label unchanged');
    assert.equal(button.icon, './icon.png',
      'global icon unchanged');
    assert.equal(button.disabled, false,
      'global disabled unchanged');

    let state = button.state(mainWindow);

    assert.equal(state.label, 'my button',
      'previous window label unchanged');
    assert.equal(state.icon, './icon.png',
      'previous window icon unchanged');
    assert.equal(state.disabled, false,
      'previous window disabled unchanged');

    let state = button.state(activeWindow);

    assert.equal(state.label, 'New label',
      'active window label updated');
    assert.equal(state.icon, './new-icon.png',
      'active window icon updated');
    assert.equal(state.disabled, true,
      'active disabled updated');

    // change the global state, only the windows without a state are affected

    button.label = 'A good label';

    assert.equal(button.label, 'A good label',
      'global label updated');
    assert.equal(button.state(mainWindow).label, 'A good label',
      'previous window label updated');
    assert.equal(button.state(activeWindow).label, 'New label',
      'active window label unchanged');

    // delete the window state will inherits the global state again

    button.state(activeWindow, null);

    assert.equal(button.state(activeWindow).label, 'A good label',
      'active window label inherited');

    // check the nodes properties
    let node = nodes[0];
    let state = button.state(mainWindow);

    assert.equal(node.getAttribute('label'), state.label,
      'node label is correct');
    assert.equal(node.getAttribute('tooltiptext'), state.label,
      'node tooltip is correct');

    assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
      'node image is correct');
    assert.equal(node.hasAttribute('disabled'), state.disabled,
      'disabled is correct');

    let node = nodes[1];
    let state = button.state(activeWindow);

    assert.equal(node.getAttribute('label'), state.label,
      'node label is correct');
    assert.equal(node.getAttribute('tooltiptext'), state.label,
      'node tooltip is correct');

    assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
      'node image is correct');
    assert.equal(node.hasAttribute('disabled'), state.disabled,
      'disabled is correct');

    return window;
  }).
  then(close).
  then(loader.unload).
  then(done, assert.fail);
};


exports['test button tab state'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let tabs = loader.require('sdk/tabs');

  let button = ToggleButton({
    id: 'my-button-7',
    label: 'my button',
    icon: './icon.png'
  });

  let mainTab = tabs.activeTab;
  let node = getWidget(button.id).node;

  tabs.open({
    url: 'about:blank',
    onActivate: function onActivate(tab) {
      tab.removeListener('activate', onActivate);

      let { activeWindow } = browserWindows;
      // set window state
      button.state(activeWindow, {
        label: 'Window label',
        icon: './window-icon.png'
      });

      // set previous active tab state
      button.state(mainTab, {
        label: 'Tab label',
        icon: './tab-icon.png',
      });

      // set current active tab state
      button.state(tab, {
        icon: './another-tab-icon.png',
        disabled: true
      });

      // check the states

      Cu.schedulePreciseGC(() => {
        assert.equal(button.label, 'my button',
          'global label unchanged');
        assert.equal(button.icon, './icon.png',
          'global icon unchanged');
        assert.equal(button.disabled, false,
          'global disabled unchanged');

        let state = button.state(mainTab);

        assert.equal(state.label, 'Tab label',
          'previous tab label updated');
        assert.equal(state.icon, './tab-icon.png',
          'previous tab icon updated');
        assert.equal(state.disabled, false,
          'previous tab disabled unchanged');

        let state = button.state(tab);

        assert.equal(state.label, 'Window label',
          'active tab inherited from window state');
        assert.equal(state.icon, './another-tab-icon.png',
          'active tab icon updated');
        assert.equal(state.disabled, true,
          'active disabled updated');

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

        // delete the window state
        button.state(activeWindow, null);

        assert.equal(button.state(tab).icon, './good-icon.png',
          'tab icon inherited from global');

        // check the node properties

        let state = button.state(tabs.activeTab);

        assert.equal(node.getAttribute('label'), state.label,
          'node label is correct');
        assert.equal(node.getAttribute('tooltiptext'), state.label,
          'node tooltip is correct');
        assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
          'node image is correct');
        assert.equal(node.hasAttribute('disabled'), state.disabled,
          'disabled is correct');

        tabs.once('activate', () => {
          // This is made in order to avoid to check the node before it
          // is updated, need a better check
          setTimeout(() => {
            let state = button.state(mainTab);

            assert.equal(node.getAttribute('label'), state.label,
              'node label is correct');
            assert.equal(node.getAttribute('tooltiptext'), state.label,
              'node tooltip is correct');
            assert.equal(node.getAttribute('image'), data.url(state.icon.substr(2)),
              'node image is correct');
            assert.equal(node.hasAttribute('disabled'), state.disabled,
              'disabled is correct');

            tab.close(() => {
              loader.unload();
              done();
            });
          }, 500);
        });

        mainTab.activate();
      });
    }
  });

};

exports['test button click'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let labels = [];

  let button = ToggleButton({
    id: 'my-button-8',
    label: 'my button',
    icon: './icon.png',
    onClick: ({label}) => labels.push(label)
  });

  let mainWindow = browserWindows.activeWindow;
  let chromeWindow = getMostRecentBrowserWindow();

  openBrowserWindow().then(focus).then(window => {
    button.state(mainWindow, { label: 'nothing' });
    button.state(mainWindow.tabs.activeTab, { label: 'foo'})
    button.state(browserWindows.activeWindow, { label: 'bar' });

    button.click();

    focus(chromeWindow).then(() => {
      button.click();

      assert.deepEqual(labels, ['bar', 'foo'],
        'button click works');

      close(window).
        then(loader.unload).
        then(done, assert.fail);
    });
  }).then(null, assert.fail);
}

exports['test button icon set'] = function(assert) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  // Test remote icon set
  assert.throws(
    () => ToggleButton({
      id: 'my-button-10',
      label: 'my button',
      icon: {
        '16': 'http://www.mozilla.org/favicon.ico'
      }
    }),
    /^The option "icon"/,
    'throws on no valid icon given');

  let button = ToggleButton({
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
  let { devicePixelRatio } = node.ownerDocument.defaultView;

  let size = 16 * devicePixelRatio;

  assert.equal(node.getAttribute('image'), data.url(button.icon[size].substr(2)),
    'the icon is set properly in navbar');

  let size = 32 * devicePixelRatio;

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

exports['test button icon se with only one option'] = function(assert) {
  const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');

  // Test remote icon set
  assert.throws(
    () => ToggleButton({
      id: 'my-button-10',
      label: 'my button',
      icon: {
        '16': 'http://www.mozilla.org/favicon.ico'
      }
    }),
    /^The option "icon"/,
    'throws on no valid icon given');

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ToggleButton({
    id: 'my-button-12',
    label: 'my button',
    icon: './icon.png'
  })

  let state = button.state(button);

  assert.throws(
    () => button.state(button, { icon: 'http://www.mozilla.org/favicon.ico' }),
    /^The option "icon"/,
    'throws on remote icon given');

  loader.unload();
};

exports['test button are not in private windows'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let{ isPrivate } = loader.require('sdk/private-browsing');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let tabs = loader.require('sdk/tabs');

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');

  let icon = {
    '16': './foo.png'
  };

  let button = ToggleButton({
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
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let { activeTab } = loader.require('sdk/tabs');

  let button = ToggleButton({
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

exports['test button checked'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let events = [];

  let button = ToggleButton({
    id: 'my-button-9',
    label: 'my button',
    icon: './icon.png',
    checked: true,
    onClick: ({label}) => events.push('clicked:' + label),
    onChange: state => events.push('changed:' + state.label + ':' + state.checked)
  });

  let { node } = getWidget(button.id);

  assert.equal(node.getAttribute('type'), 'checkbox',
    'node type is properly set');

  let mainWindow = browserWindows.activeWindow;
  let chromeWindow = getMostRecentBrowserWindow();

  openBrowserWindow().then(focus).then(window => {
    button.state(mainWindow, { label: 'nothing' });
    button.state(mainWindow.tabs.activeTab, { label: 'foo'})
    button.state(browserWindows.activeWindow, { label: 'bar' });

    button.click();
    button.click();

    focus(chromeWindow).then(() => {
      button.click();
      button.click();

      assert.deepEqual(events, [
          'clicked:bar', 'changed:bar:false', 'clicked:bar', 'changed:bar:true',
          'clicked:foo', 'changed:foo:false', 'clicked:foo', 'changed:foo:true'
        ],
        'button change events works');

      close(window).
        then(loader.unload).
        then(done, assert.fail);
    })
  }).then(null, assert.fail);
}

exports['test button is checked on window level'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');
  let tabs = loader.require('sdk/tabs');

  let button = ToggleButton({
    id: 'my-button-20',
    label: 'my button',
    icon: './icon.png'
  });

  let mainWindow = browserWindows.activeWindow;
  let mainTab = tabs.activeTab;

  assert.equal(button.checked, false,
    'global state, checked is `false`.');
  assert.equal(button.state(mainTab).checked, false,
    'tab state, checked is `false`.');
  assert.equal(button.state(mainWindow).checked, false,
    'window state, checked is `false`.');

  button.click();

  tabs.open({
    url: 'about:blank',
    onActivate: function onActivate(tab) {
      tab.removeListener('activate', onActivate);

      assert.notEqual(mainTab, tab,
        'the current tab is not the same.');

      assert.equal(button.checked, false,
        'global state, checked is `false`.');
      assert.equal(button.state(mainTab).checked, true,
        'previous tab state, checked is `true`.');
      assert.equal(button.state(tab).checked, true,
        'current tab state, checked is `true`.');
      assert.equal(button.state(mainWindow).checked, true,
        'window state, checked is `true`.');

      openBrowserWindow().then(focus).then(window => {
        let { activeWindow } = browserWindows;
        let { activeTab } = activeWindow.tabs;

        assert.equal(button.checked, false,
          'global state, checked is `false`.');
        assert.equal(button.state(activeTab).checked, false,
          'tab state, checked is `false`.');

        assert.equal(button.state(activeWindow).checked, false,
          'window state, checked is `false`.');

        tab.close(()=> {
          close(window).
            then(loader.unload).
            then(done, assert.fail);
        })
      }).
      then(null, assert.fail);
    }
  });

};

exports['test button click do not messing up states'] = function(assert) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { browserWindows } = loader.require('sdk/windows');

  let button = ToggleButton({
    id: 'my-button-21',
    label: 'my button',
    icon: './icon.png'
  });

  let mainWindow = browserWindows.activeWindow;
  let { activeTab } = mainWindow.tabs;

  button.state(mainWindow, { icon: './new-icon.png' });
  button.state(activeTab, { label: 'foo'})

  assert.equal(button.state(mainWindow).label, 'my button',
    'label property for window state, properly derived from global state');

  assert.equal(button.state(activeTab).icon, './new-icon.png',
    'icon property for tab state, properly derived from window state');

  button.click();

  button.label = 'bar';

  assert.equal(button.state(mainWindow).label, 'bar',
    'label property for window state, properly derived from global state');

  button.state(mainWindow, null);

  assert.equal(button.state(activeTab).icon, './icon.png',
    'icon property for tab state, properly derived from window state');

  loader.unload();
}

exports['test buttons can have anchored panels'] = function(assert, done) {
  let loader = Loader(module);
  let { ToggleButton } = loader.require('sdk/ui');
  let { Panel } = loader.require('sdk/panel');
  let { identify } = loader.require('sdk/ui/id');
  let { getActiveView } = loader.require('sdk/view/core');

  let b1 = ToggleButton({
    id: 'my-button-22',
    label: 'my button',
    icon: './icon.png',
    onChange: ({checked}) => checked && panel.show()
  });

  let b2 = ToggleButton({
    id: 'my-button-23',
    label: 'my button',
    icon: './icon.png',
    onChange: ({checked}) => checked && panel.show({position: b2})
  });

  let panel = Panel({
    position: b1
  });

  let { document } = getMostRecentBrowserWindow();
  let b1Node = document.getElementById(identify(b1));
  let b2Node = document.getElementById(identify(b2));
  let panelNode = getActiveView(panel);

  panel.once('show', () => {
    assert.ok(b1.state('window').checked,
      'button is checked');

    assert.equal(panelNode.getAttribute('type'), 'arrow',
      'the panel is a arrow type');

    assert.strictEqual(b1Node, panelNode.anchorNode,
      'the panel is anchored properly to the button given in costructor');

    panel.hide();

    panel.once('show', () => {
      assert.ok(b2.state('window').checked,
        'button is checked');

      assert.equal(panelNode.getAttribute('type'), 'arrow',
        'the panel is a arrow type');

      // test also that the button passed in `show` method, takes the precedence
      // over the button set in panel's constructor.
      assert.strictEqual(b2Node, panelNode.anchorNode,
        'the panel is anchored properly to the button passed to show method');

      loader.unload();

      done();
    });

    b2.click();
  });

  b1.click();
}

require('sdk/test').run(exports);
