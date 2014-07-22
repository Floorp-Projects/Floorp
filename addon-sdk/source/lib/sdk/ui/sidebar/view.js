/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable',
  'engines': {
    'Firefox': '*'
  }
};

const { models, buttons, views, viewsFor, modelFor } = require('./namespace');
const { isBrowser, getMostRecentBrowserWindow, windows, isWindowPrivate } = require('../../window/utils');
const { setStateFor } = require('../state');
const { defer } = require('../../core/promise');
const { isPrivateBrowsingSupported, data } = require('../../self');

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const WEB_PANEL_BROWSER_ID = 'web-panels-browser';

const resolveURL = (url) => url ? data.url(url) : url;

function create(window, details) {
  let id = makeID(details.id);
  let { document } = window;

  if (document.getElementById(id))
    throw new Error('The ID "' + details.id + '" seems already used.');

  let menuitem = document.createElementNS(XUL_NS, 'menuitem');
  menuitem.setAttribute('id', id);
  menuitem.setAttribute('label', details.title);
  menuitem.setAttribute('sidebarurl', resolveURL(details.sidebarurl));
  menuitem.setAttribute('checked', 'false');
  menuitem.setAttribute('type', 'checkbox');
  menuitem.setAttribute('group', 'sidebar');
  menuitem.setAttribute('autoCheck', 'false');

  document.getElementById('viewSidebarMenu').appendChild(menuitem);

  return menuitem;
}
exports.create = create;

function dispose(menuitem) {
  menuitem.parentNode.removeChild(menuitem);
}
exports.dispose = dispose;

function updateTitle(sidebar, title) {
  let button = buttons.get(sidebar);

  for (let window of windows(null, { includePrivate: true })) {
  	let { document } = window;

    // update the button
    if (button) {
      setStateFor(button, window, { label: title });
    }

    // update the menuitem
    let mi = document.getElementById(makeID(sidebar.id));
    if (mi) {
      mi.setAttribute('label', title)
    }

    // update sidebar, if showing
    if (isSidebarShowing(window, sidebar)) {
      document.getElementById('sidebar-title').setAttribute('value', title);
    }
  }
}
exports.updateTitle = updateTitle;

function updateURL(sidebar, url) {
  let eleID = makeID(sidebar.id);

  url = resolveURL(url);

  for (let window of windows(null, { includePrivate: true })) {
    // update the menuitem
    let mi = window.document.getElementById(eleID);
    if (mi) {
      mi.setAttribute('sidebarurl', url)
    }

    // update sidebar, if showing
    if (isSidebarShowing(window, sidebar)) {
      showSidebar(window, sidebar, url);
    }
  }
}
exports.updateURL = updateURL;

function isSidebarShowing(window, sidebar) {
  let win = window || getMostRecentBrowserWindow();

  // make sure there is a window
  if (!win) {
    return false;
  }

  // make sure there is a sidebar for the window
  let sb = win.document.getElementById('sidebar');
  let sidebarTitle = win.document.getElementById('sidebar-title');
  if (!(sb && sidebarTitle)) {
    return false;
  }

  // checks if the sidebar box is hidden
  let sbb = win.document.getElementById('sidebar-box');
  if (!sbb || sbb.hidden) {
    return false;
  }

  if (sidebarTitle.value == modelFor(sidebar).title) {
    let url = resolveURL(modelFor(sidebar).url);

    // checks if the sidebar is loading
    if (win.gWebPanelURI == url) {
      return true;
    }

    // checks if the sidebar loaded already
    let ele = sb.contentDocument && sb.contentDocument.getElementById(WEB_PANEL_BROWSER_ID);
    if (!ele) {
      return false;
    }

    if (ele.getAttribute('cachedurl') == url) {
      return true;
    }

    if (ele && ele.contentWindow && ele.contentWindow.location == url) {
      return true;
    }
  }

  // default
  return false;
}
exports.isSidebarShowing = isSidebarShowing;

function showSidebar(window, sidebar, newURL) {
  window = window || getMostRecentBrowserWindow();

  let { promise, resolve, reject } = defer();
  let model = modelFor(sidebar);

  if (!newURL && isSidebarShowing(window, sidebar)) {
    resolve({});
  }
  else if (!isPrivateBrowsingSupported && isWindowPrivate(window)) {
    reject(Error('You cannot show a sidebar on private windows'));
  }
  else {
    sidebar.once('show', resolve);

    let menuitem = window.document.getElementById(makeID(model.id));
    menuitem.setAttribute('checked', true);

    window.openWebPanel(model.title, resolveURL(newURL || model.url));
  }

  return promise;
}
exports.showSidebar = showSidebar;


function hideSidebar(window, sidebar) {
  window = window || getMostRecentBrowserWindow();

  let { promise, resolve, reject } = defer();

  if (!isSidebarShowing(window, sidebar)) {
    reject(Error('The sidebar is already hidden'));
  }
  else {
    sidebar.once('hide', resolve);

    // Below was taken from http://mxr.mozilla.org/mozilla-central/source/browser/base/content/browser.js#4775
    // the code for window.todggleSideBar()..
    let { document } = window;
    let sidebarEle = document.getElementById('sidebar');
    let sidebarTitle = document.getElementById('sidebar-title');
    let sidebarBox = document.getElementById('sidebar-box');
    let sidebarSplitter = document.getElementById('sidebar-splitter');
    let commandID = sidebarBox.getAttribute('sidebarcommand');
    let sidebarBroadcaster = document.getElementById(commandID);

    sidebarBox.hidden = true;
    sidebarSplitter.hidden = true;

    sidebarEle.setAttribute('src', 'about:blank');
    //sidebarEle.docShell.createAboutBlankContentViewer(null);

    sidebarBroadcaster.removeAttribute('checked');
    sidebarBox.setAttribute('sidebarcommand', '');
    sidebarTitle.value = '';
    sidebarBox.hidden = true;
    sidebarSplitter.hidden = true;

    // TODO: perhaps this isn't necessary if the window is not most recent?
    window.gBrowser.selectedBrowser.focus();
  }

  return promise;
}
exports.hideSidebar = hideSidebar;

function makeID(id) {
  return 'jetpack-sidebar-' + id;
}
