/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cu } = require('chrome');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { fromIterator } = require('sdk/util/array');

const BUILTIN_SIDEBAR_MENUITEMS = exports.BUILTIN_SIDEBAR_MENUITEMS = [
  'menu_socialSidebar',
  'menu_historySidebar',
  'menu_bookmarksSidebar',
  'menu_tabsSidebar',
];

function isSidebarShowing(window) {
  window = window || getMostRecentBrowserWindow();
  let sidebar = window.document.getElementById('sidebar-box');
  return !sidebar.hidden;
}
exports.isSidebarShowing = isSidebarShowing;

function getSidebarMenuitems(window) {
  window = window || getMostRecentBrowserWindow();
  return fromIterator(window.document.querySelectorAll('#viewSidebarMenu menuitem'));
}
exports.getSidebarMenuitems = getSidebarMenuitems;

function getExtraSidebarMenuitems() {
  let menuitems = getSidebarMenuitems();
  return menuitems.filter(function(mi) {
    return BUILTIN_SIDEBAR_MENUITEMS.indexOf(mi.getAttribute('id')) < 0;
  });
}
exports.getExtraSidebarMenuitems = getExtraSidebarMenuitems;

function makeID(id) {
  return 'jetpack-sidebar-' + id;
}
exports.makeID = makeID;

function simulateCommand(ele) {
  let window = ele.ownerGlobal;
  let { document } = window;
  var evt = document.createEvent('XULCommandEvent');
  evt.initCommandEvent('command', true, true, window,
    0, false, false, false, false, null, 0);
  ele.dispatchEvent(evt);
}
exports.simulateCommand = simulateCommand;

function simulateClick(ele) {
  let window = ele.ownerGlobal;
  let { document } = window;
  let evt = document.createEvent('MouseEvents');
  evt.initMouseEvent('click', true, true, window,
    0, 0, 0, 0, 0, false, false, false, false, 0, null);
  ele.dispatchEvent(evt);
}
exports.simulateClick = simulateClick;

// OSX and Windows exhibit different behaviors when 'checked' is false,
// so compare against the consistent 'true'. See bug 894809.
function isChecked(node) {
  return node.getAttribute('checked') === 'true';
};
exports.isChecked = isChecked;
