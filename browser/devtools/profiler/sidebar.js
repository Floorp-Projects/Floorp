/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Cu } = require("chrome");
let EventEmitter = require("devtools/shared/event-emitter");

Cu.import("resource:///modules/devtools/SideMenuWidget.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

const {
  PROFILE_IDLE,
  PROFILE_COMPLETED,
  PROFILE_RUNNING,
  L10N_BUNDLE
} = require("devtools/profiler/consts");

loader.lazyGetter(this, "L10N", () => new ViewHelpers.L10N(L10N_BUNDLE));

function Sidebar(el) {
  EventEmitter.decorate(this);

  this.document = el.ownerDocument;
  this.widget = new SideMenuWidget(el);
  this.widget.notice = L10N.getStr("profiler.sidebarNotice");
}

Sidebar.prototype = Heritage.extend(WidgetMethods, {
  addProfile: function (profile) {
    let doc  = this.document;
    let box  = doc.createElement("vbox");
    let h3   = doc.createElement("h3");
    let span = doc.createElement("span");

    box.id = "profile-" + profile.uid;
    box.className = "profiler-sidebar-item";

    h3.textContent = profile.name;
    span.textContent = L10N.getStr("profiler.stateIdle");

    box.appendChild(h3);
    box.appendChild(span);

    this.push([box], {
      attachment: {
        uid:   profile.uid,
        name:  profile.name,
        state: PROFILE_IDLE
      }
    });
  },

  getElementByProfile: function (profile) {
    return this.document.querySelector("#profile-" + profile.uid);
  },

  getItemByProfile: function (profile) {
    return this.getItemForPredicate(item => item.attachment.uid === profile.uid);
  },

  setProfileState: function (profile, state) {
    let item = this.getItemByProfile(profile);
    let label = item.target.querySelector(".profiler-sidebar-item > span");

    switch (state) {
      case PROFILE_IDLE:
        label.textContent = L10N.getStr("profiler.stateIdle");
        break;
      case PROFILE_RUNNING:
        label.textContent = L10N.getStr("profiler.stateRunning");
        break;
      case PROFILE_COMPLETED:
        label.textContent = L10N.getStr("profiler.stateCompleted");
        break;
      default: // Wrong state, do nothing.
        return;
    }

    item.attachment.state = state;
    this.emit("stateChanged", item);
  }
});

module.exports = Sidebar;