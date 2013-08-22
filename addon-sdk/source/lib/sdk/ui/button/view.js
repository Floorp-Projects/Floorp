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

const { Cu } = require('chrome');
const { on, off, emit } = require('../../event/core');

const { id: addonID, data } = require('sdk/self');
const buttonPrefix = 'button--' + addonID.replace(/@/g, '-at-');

const { isObject } = require('../../lang/type');

const { getMostRecentBrowserWindow } = require('../../window/utils');
const { ignoreWindow } = require('../../private-browsing/utils');
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { AREA_PANEL, AREA_NAVBAR } = CustomizableUI;

const SIZE = {
  'small': 16,
  'medium': 32,
  'large': 64
}

const XUL_NS = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

const toWidgetID = function(id) buttonPrefix + '-' + id;
const toButtonID = function(id) id.substr(buttonPrefix.length + 1);

const views = {};

const buttonListener = {
  onWidgetAdded: function(widgetId, area, position) {
    let id = toButtonID(widgetId);

    if (id in views && views[id].area !== area) {
      views[id].area = area;
      emit(views[id].events, 'moved', area === AREA_PANEL);
    }
  }
};

CustomizableUI.addListener(buttonListener);

require('../../system/unload').when(function(){
  CustomizableUI.removeListener(buttonListener);
});

function getNode(id, window) {
  return !(id in views) || ignoreWindow(window)
    ? null
    : CustomizableUI.getWidget(toWidgetID(id)).forWindow(window).node
};

function isInPanel(id) views[id] && views[id].area === AREA_PANEL;

function getImage(icon, areaIsPanel, pixelRatio) {
  let targetSize = (areaIsPanel ? 32 : 18) * pixelRatio;
  let bestSize = 0;
  let image = icon;

  if (isObject(icon)) {
    for (let size of Object.keys(icon)) {
      size = +size;
      let offset = targetSize - size;

      if (offset === 0) {
        bestSize = size;
        break;
      }

      let delta = Math.abs(offset) - Math.abs(targetSize - bestSize);

      if (delta < 0)
        bestSize = size;
    }

    image = icon[bestSize];
  }

  if (image.indexOf('./') === 0)
    return data.url(image.substr(2));

  return image;
}

function create(options) {
  let { id, label, image, size, type } = options;
  let bus = {};

  if (id in views)
    throw new Error('The ID "' + id + '" seems already used.');

  CustomizableUI.createWidget({
    id: toWidgetID(id),
    type: 'custom',
    removable: true,
    defaultArea: AREA_NAVBAR,
    allowedAreas: [ AREA_PANEL, AREA_NAVBAR ],

    onBuild: function(document) {
      let window = document.defaultView;

      let node = document.createElementNS(XUL_NS, 'toolbarbutton');

      if (ignoreWindow(window))
        node.style.display = 'none';

      node.setAttribute('id', this.id);
      node.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional');
      node.setAttribute('width', SIZE[size] || 16);
      node.setAttribute('type', type);

      views[id] = {
        events: bus,
        area: this.currentArea
      };

      node.addEventListener('command', function(event) {
        if (views[id])
          emit(views[id].events, 'click', event);
      });

      return node;
    }
  });

  return bus;
};
exports.create = create;

function dispose({id}) {
  if (!views[id]) return;

  off(views[id].events);
  delete views[id];
  CustomizableUI.destroyWidget(toWidgetID(id));
}
exports.dispose = dispose;

function setIcon({id}, window, icon) {
  let node = getNode(id, window);

  if (node) {
    let image = getImage(icon, isInPanel(id), window.devicePixelRatio);

    node.setAttribute('image', image);
  }
}
exports.setIcon = setIcon;

function setLabel({id}, window, label) {
  let node = getNode(id, window);

  if (node) {
    node.setAttribute('label', label);
    node.setAttribute('tooltiptext', label);
  }
}
exports.setLabel = setLabel;

function setDisabled({id}, window, disabled) {
  let node = getNode(id, window);

  if (node) {
    if (disabled)
      node.setAttribute('disabled', disabled);
    else
      node.removeAttribute('disabled');
  }
}
exports.setDisabled = setDisabled;

function setChecked({id}, window, checked) {
  let node = getNode(id, window);

  if (node) {
    if (checked)
      node.setAttribute('checked', checked);
    else
      node.removeAttribute('checked');
  }
}
exports.setChecked = setChecked;

function click({id}) {
  let window = getMostRecentBrowserWindow();

  let node = getNode(id, window);

  if (node)
    node.click();
}
exports.click = click;
