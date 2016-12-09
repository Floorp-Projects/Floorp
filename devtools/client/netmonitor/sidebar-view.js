/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ./netmonitor-controller.js */
/* globals dumpn, $, NetMonitorView */

"use strict";

const { Task } = require("devtools/shared/task");
const { EVENTS } = require("./events");

/**
 * Functions handling the sidebar details view.
 */
function SidebarView() {
  dumpn("SidebarView was instantiated");
}

SidebarView.prototype = {
  /**
   * Sets this view hidden or visible. It's visible by default.
   *
   * @param boolean visibleFlag
   *        Specifies the intended visibility.
   */
  toggle: function (visibleFlag) {
    NetMonitorView.toggleDetailsPane({ visible: visibleFlag });
    NetMonitorView.RequestsMenu._flushWaterfallViews(true);
  },

  /**
   * Populates this view with the specified data.
   *
   * @param object data
   *        The data source (this should be the attachment of a request item).
   * @return object
   *        Returns a promise that resolves upon population of the subview.
   */
  populate: Task.async(function* (data) {
    let isCustom = data.isCustom;
    let view = isCustom ?
      NetMonitorView.CustomRequest :
      NetMonitorView.NetworkDetails;

    yield view.populate(data);
    $("#details-pane").selectedIndex = isCustom ? 0 : 1;

    window.emit(EVENTS.SIDEBAR_POPULATED);
  })

};

exports.SidebarView = SidebarView;
