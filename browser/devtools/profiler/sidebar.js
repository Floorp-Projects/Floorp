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

let stopProfilingString = L10N.getStr("profiler.stopProfilerString");
let startProfilingString = L10N.getStr("profiler.startProfilerString");

function Sidebar(el) {
  EventEmitter.decorate(this);

  this.document = el.ownerDocument;
  this.widget = new SideMenuWidget(el, { showArrows: true });
  this.widget.notice = L10N.getStr("profiler.sidebarNotice");

  this.widget.addEventListener("select", (ev) => {
    if (!ev.detail)
      return;

    this.emit("select", parseInt(ev.detail.value, 10));
  });
}

Sidebar.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Adds a new item for a profile to the sidebar. Markup
   * example:
   *
   *   <vbox id="profile-1" class="profiler-sidebar-item">
   *     <h3>Profile 1</h3>
   *     <hbox>
   *       <span flex="1">Completed</span>
   *       <a>Save</a>
   *     </hbox>
   *   </vbox>
   *
   */
  addProfile: function (profile) {
    let doc  = this.document;
    let vbox = doc.createElement("vbox");
    let hbox = doc.createElement("hbox");
    let h3   = doc.createElement("h3");
    let span = doc.createElement("span");
    let save = doc.createElement("a");

    vbox.id = "profile-" + profile.uid;
    vbox.className = "profiler-sidebar-item";

    h3.textContent = profile.name;
    span.setAttribute("flex", 1);
    span.textContent = L10N.getStr("profiler.stateIdle");

    save.textContent = L10N.getStr("profiler.save");
    save.addEventListener("click", (ev) => {
      ev.preventDefault();
      this.emit("save", profile.uid);
    });

    hbox.appendChild(span);
    hbox.appendChild(save);

    vbox.appendChild(h3);
    vbox.appendChild(hbox);

    this.push([vbox, profile.uid], {
      attachment: {
        name:  profile.name,
        state: PROFILE_IDLE
      }
    });
  },

  getElementByProfile: function (profile) {
    return this.document.querySelector("#profile-" + profile.uid);
  },

  getItemByProfile: function (profile) {
    return this.getItemByValue(profile.uid.toString());
  },

  setProfileState: function (profile, state) {
    let item = this.getItemByProfile(profile);
    let doc = this.document;
    let label = item.target.querySelector(".profiler-sidebar-item > hbox > span");
    let toggleButton = doc.getElementById("profiler-start");

    switch (state) {
      case PROFILE_IDLE:
        item.target.setAttribute("state", "idle");
        label.textContent = L10N.getStr("profiler.stateIdle");
        break;
      case PROFILE_RUNNING:
        item.target.setAttribute("state", "running");
        label.textContent = L10N.getStr("profiler.stateRunning");
        toggleButton.setAttribute("tooltiptext",stopProfilingString);
        break;
      case PROFILE_COMPLETED:
        item.target.setAttribute("state", "completed");
        label.textContent = L10N.getStr("profiler.stateCompleted");
        toggleButton.setAttribute("tooltiptext",startProfilingString);
        break;
      default: // Wrong state, do nothing.
        return;
    }

    item.attachment.state = state;
    this.emit("stateChanged", item);
  }
});

module.exports = Sidebar;
