/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Firefox': '*'
  }
};

const { Class } = require('../core/heritage');
const { merge } = require('../util/object');
const { Disposable } = require('../core/disposable');
const { off, emit, setListeners } = require('../event/core');
const { EventTarget } = require('../event/target');
const { URL } = require('../url');
const { add, remove, has, clear, iterator } = require('../lang/weak-set');
const { id: addonID, data } = require('../self');
const { WindowTracker } = require('../deprecated/window-utils');
const { isShowing } = require('./sidebar/utils');
const { isBrowser, getMostRecentBrowserWindow, windows, isWindowPrivate } = require('../window/utils');
const { ns } = require('../core/namespace');
const { remove: removeFromArray } = require('../util/array');
const { show, hide, toggle } = require('./sidebar/actions');
const { Worker } = require('../content/worker');
const { contract: sidebarContract } = require('./sidebar/contract');
const { create, dispose, updateTitle, updateURL, isSidebarShowing, showSidebar, hideSidebar } = require('./sidebar/view');
const { defer } = require('../core/promise');
const { models, views, viewsFor, modelFor } = require('./sidebar/namespace');
const { isLocalURL } = require('../url');
const { ensure } = require('../system/unload');
const { identify } = require('./id');
const { uuid } = require('../util/uuid');
const { viewFor } = require('../view/core');

const resolveURL = (url) => url ? data.url(url) : url;

const sidebarNS = ns();

const WEB_PANEL_BROWSER_ID = 'web-panels-browser';

let sidebars = {};

const Sidebar = Class({
  implements: [ Disposable ],
  extends: EventTarget,
  setup: function(options) {
    // inital validation for the model information
    let model = sidebarContract(options);

    // save the model information
    models.set(this, model);

    // generate an id if one was not provided
    model.id = model.id || addonID + '-' + uuid();

    // further validation for the title and url
    validateTitleAndURLCombo({}, this.title, this.url);

    const self = this;
    const internals = sidebarNS(self);
    const windowNS = internals.windowNS = ns();

    // see bug https://bugzilla.mozilla.org/show_bug.cgi?id=886148
    ensure(this, 'destroy');

    setListeners(this, options);

    let bars = [];
    internals.tracker = WindowTracker({
      onTrack: function(window) {
        if (!isBrowser(window))
          return;

        let sidebar = window.document.getElementById('sidebar');
        let sidebarBox = window.document.getElementById('sidebar-box');

        let bar = create(window, {
          id: self.id,
          title: self.title,
          sidebarurl: self.url
        });
        bars.push(bar);
        windowNS(window).bar = bar;

        bar.addEventListener('command', function() {
          if (isSidebarShowing(window, self)) {
            hideSidebar(window, self);
            return;
          }

          showSidebar(window, self);
        }, false);

        function onSidebarLoad() {
          // check if the sidebar is ready
          let isReady = sidebar.docShell && sidebar.contentDocument;
          if (!isReady)
            return;

          // check if it is a web panel
          let panelBrowser = sidebar.contentDocument.getElementById(WEB_PANEL_BROWSER_ID);
          if (!panelBrowser) {
            bar.removeAttribute('checked');
            return;
          }

          let sbTitle = window.document.getElementById('sidebar-title');
          function onWebPanelSidebarCreated() {
            if (panelBrowser.contentWindow.location != resolveURL(model.url) ||
                sbTitle.value != model.title) {
              return;
            }

            let worker = windowNS(window).worker = Worker({
              window: panelBrowser.contentWindow,
              injectInDocument: true
            });

            function onWebPanelSidebarUnload() {
              windowNS(window).onWebPanelSidebarUnload = null;

              // uncheck the associated menuitem
              bar.setAttribute('checked', 'false');

              emit(self, 'hide', {});
              emit(self, 'detach', worker);
              windowNS(window).worker = null;
            }
            windowNS(window).onWebPanelSidebarUnload = onWebPanelSidebarUnload;
            panelBrowser.contentWindow.addEventListener('unload', onWebPanelSidebarUnload, true);

            // check the associated menuitem
            bar.setAttribute('checked', 'true');

            function onWebPanelSidebarReady() {
              panelBrowser.contentWindow.removeEventListener('DOMContentLoaded', onWebPanelSidebarReady, false);
              windowNS(window).onWebPanelSidebarReady = null;

              emit(self, 'ready', worker);
            }
            windowNS(window).onWebPanelSidebarReady = onWebPanelSidebarReady;
            panelBrowser.contentWindow.addEventListener('DOMContentLoaded', onWebPanelSidebarReady, false);

            function onWebPanelSidebarLoad() {
              panelBrowser.contentWindow.removeEventListener('load', onWebPanelSidebarLoad, true);
              windowNS(window).onWebPanelSidebarLoad = null;

              // TODO: decide if returning worker is acceptable..
              //emit(self, 'show', { worker: worker });
              emit(self, 'show', {});
            }
            windowNS(window).onWebPanelSidebarLoad = onWebPanelSidebarLoad;
            panelBrowser.contentWindow.addEventListener('load', onWebPanelSidebarLoad, true);

            emit(self, 'attach', worker);
          }
          windowNS(window).onWebPanelSidebarCreated = onWebPanelSidebarCreated;
          panelBrowser.addEventListener('DOMWindowCreated', onWebPanelSidebarCreated, true);
        }
        windowNS(window).onSidebarLoad = onSidebarLoad;
        sidebar.addEventListener('load', onSidebarLoad, true); // removed properly
      },
      onUntrack: function(window) {
        if (!isBrowser(window))
          return;

        // hide the sidebar if it is showing
        hideSidebar(window, self);

        // kill the menu item
        let { bar } = windowNS(window);
        if (bar) {
          removeFromArray(viewsFor(self), bar);
          dispose(bar);
        }

        // kill listeners
        let sidebar = window.document.getElementById('sidebar');

        if (windowNS(window).onSidebarLoad) {
          sidebar && sidebar.removeEventListener('load', windowNS(window).onSidebarLoad, true)
          windowNS(window).onSidebarLoad = null;
        }

        let panelBrowser = sidebar && sidebar.contentDocument.getElementById(WEB_PANEL_BROWSER_ID);
        if (windowNS(window).onWebPanelSidebarCreated) {
          panelBrowser && panelBrowser.removeEventListener('DOMWindowCreated', windowNS(window).onWebPanelSidebarCreated, true);
          windowNS(window).onWebPanelSidebarCreated = null;
        }

        if (windowNS(window).onWebPanelSidebarReady) {
          panelBrowser && panelBrowser.contentWindow.removeEventListener('DOMContentLoaded', windowNS(window).onWebPanelSidebarReady, false);
          windowNS(window).onWebPanelSidebarReady = null;
        }

        if (windowNS(window).onWebPanelSidebarLoad) {
          panelBrowser && panelBrowser.contentWindow.removeEventListener('load', windowNS(window).onWebPanelSidebarLoad, true);
          windowNS(window).onWebPanelSidebarLoad = null;
        }

        if (windowNS(window).onWebPanelSidebarUnload) {
          panelBrowser && panelBrowser.contentWindow.removeEventListener('unload', windowNS(window).onWebPanelSidebarUnload, true);
          windowNS(window).onWebPanelSidebarUnload();
        }
      }
    });

    views.set(this, bars);

    add(sidebars, this);
  },
  get id() (modelFor(this) || {}).id,
  get title() (modelFor(this) || {}).title,
  set title(v) {
    // destroyed?
    if (!modelFor(this))
      return;
    // validation
    if (typeof v != 'string')
      throw Error('title must be a string');
    validateTitleAndURLCombo(this, v, this.url);
    // do update
    updateTitle(this, v);
    return modelFor(this).title = v;
  },
  get url() (modelFor(this) || {}).url,
  set url(v) {
    // destroyed?
    if (!modelFor(this))
      return;

    // validation
    if (!isLocalURL(v))
      throw Error('the url must be a valid local url');

    validateTitleAndURLCombo(this, this.title, v);

    // do update
    updateURL(this, v);
    modelFor(this).url = v;
  },
  show: function(window) showSidebar(viewFor(window), this),
  hide: function(window) hideSidebar(viewFor(window), this),
  dispose: function() {
    const internals = sidebarNS(this);

    off(this);

    remove(sidebars, this);

    // stop tracking windows
    if (internals.tracker) {
      internals.tracker.unload();
    }

    internals.tracker = null;
    internals.windowNS = null;

    views.delete(this);
    models.delete(this);
  }
});
exports.Sidebar = Sidebar;

function validateTitleAndURLCombo(sidebar, title, url) {
  url = resolveURL(url);
  
  if (sidebar.title == title && sidebar.url == url) {
    return false;
  }

  for (let window of windows(null, { includePrivate: true })) {
    let sidebar = window.document.querySelector('menuitem[sidebarurl="' + url + '"][label="' + title + '"]');
    if (sidebar) {
      throw Error('The provided title and url combination is invalid (already used).');
    }
  }

  return false;
}

isShowing.define(Sidebar, isSidebarShowing.bind(null, null));
show.define(Sidebar, showSidebar.bind(null, null));
hide.define(Sidebar, hideSidebar.bind(null, null));

identify.define(Sidebar, function(sidebar) {
  return sidebar.id;
});

function toggleSidebar(window, sidebar) {
  // TODO: make sure this is not private
  window = window || getMostRecentBrowserWindow();
  if (isSidebarShowing(window, sidebar)) {
    return hideSidebar(window, sidebar);
  }
  return showSidebar(window, sidebar);
}
toggle.define(Sidebar, toggleSidebar.bind(null, null));
