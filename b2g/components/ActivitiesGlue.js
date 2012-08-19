/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function ActivitiesDialog() {
  this._id = 0;

  this.activities = [];
}

ActivitiesDialog.prototype = {
  run: function ap_run() {
    let id = "activity-choice" + this._id++;
    let activity = this.activities.shift();

    let choices = [];
    activity.list.forEach(function(item) {
      choices.push({ title: item.title, icon: item.icon });
    });


    // Keep up the frond-end of an activity choice. The messages contains
    // a list of {names, icons} for applications able to handle this particular
    // activity. The front-end should display a UI to pick one.
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    let event = content.document.createEvent("CustomEvent");
    event.initCustomEvent("mozChromeEvent", true, true, {
      type: "activity-choice",
      id: id,
      name: activity.name,
      choices: choices
    });

    // Listen the resulting choice from the front-end. If there is no choice,
    // let's return -1, which means the user has cancelled the dialog.
    content.addEventListener("mozContentEvent", function act_getChoice(evt) {
      if (evt.detail.id != id)
        return;

      content.removeEventListener("mozContentEvent", act_getChoice);
      activity.callback.handleEvent(evt.detail.value ? evt.detail.value : -1);
    });

    content.dispatchEvent(event);
  },

  chooseActivity: function ap_chooseActivity(aName, aActivities, aCallback) {
    this.activities.push({
      name: aName,
      list: aActivities,
      callback: aCallback
    });
    Services.tm.currentThread.dispatch(this, Ci.nsIEventTarget.DISPATCH_NORMAL);
  },

  classID: Components.ID("{70a83123-7467-4389-a309-3e81c74ad002}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIActivityUIGlue, Ci.nsIRunnable])
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivitiesDialog]);

