/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const tabs = require('tabs');
const widgets = require('widget');
const data = require('self').data;
const pageMod = require('page-mod');
const panel = require('panel');

const ICON_WIDTH = 16;

function updateWidgetView(tab) {
  let widgetView = widget.getView(tab.window);
  if (!tab.libraries) {
    tab.libraries = [];
  }
  widgetView.port.emit("update", tab.libraries);
  widgetView.width = tab.libraries.length * ICON_WIDTH;
}

var widget = widgets.Widget({
  id: "library-detector",
  label: "Library Detector",
  contentURL: data.url("widget.html"),
  panel: panel.Panel({
    width: 240,
    height: 60,
    contentURL: data.url("panel.html")
  })
});

widget.port.on('setLibraryInfo', function(libraryInfo) {
  widget.panel.postMessage(libraryInfo);
});

pageMod.PageMod({
  include: "*",
  contentScriptWhen: 'end',
  contentScriptFile: (data.url('library-detector.js')),
  onAttach: function(worker) {
    worker.on('message', function(libraryList) {
      if (!worker.tab.libraries) {
        worker.tab.libraries = [];
      }
      libraryList.forEach(function(library) {
        if (worker.tab.libraries.indexOf(library) == -1) {
          worker.tab.libraries.push(library);
        }
      });
      if (worker.tab == tabs.activeTab) {
        updateWidgetView(worker.tab);
      }
    });
  }
});

tabs.on('activate', function(tab) {
  updateWidgetView(tab);
});

/*
For change of location
*/
tabs.on('ready', function(tab) {
  tab.libraries = [];
});