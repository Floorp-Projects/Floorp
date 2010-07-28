/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TabView Tabs.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Edward Lee <edilee@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let EXPORTED_SYMBOLS = ["Tabs"];

let Tabs = let (T = {
  //////////////////////////////////////////////////////////////////////////////
  //// Public
  //////////////////////////////////////////////////////////////////////////////

  /**
   * Get an array of all tabs from all tabbrowser windows.
   *
   * @usage let numTabs = Tabs.allTabs.length;
   *        Tabs.allTabs.forEach(handleTabs);
   */
  get allTabs() {
    // Get tabs from each browser and flatten them into one array
    return Array.concat.apply({}, T.allBrowsers.map(function(browser) {
      return Array.slice(browser.gBrowser.tabs);
    }));
  },

  /**
   * Attach a callback for when a tab's attributes change such as title, busy
   * state, icon, etc.
   *
   * There is also an unbind method off of this method to remove the callback.
   *
   * @param callback
   *        Callback that gets called with the tab being changed as "this" and
   *        the event as the first argument.
   * @usage Tabs.onChange(handleChange);
   *        Tabs.onChange.unbind(handleChange);
   */
  get onChange() T.makeBind("onChange"),

  /**
   * Attach a callback for when a tab is closed.
   *
   * There is also an unbind method off of this method to remove the callback.
   *
   * @param callback
   *        Callback that gets called with the tab being closed as "this" and
   *        the event as the first argument.
   * @usage Tabs.onClose(handleClose);
   *        Tabs.onClose.unbind(handleClose);
   */
  get onClose() T.makeBind("onClose"),

  /**
   * Attach a callback for when a tab is moved.
   *
   * There is also an unbind method off of this method to remove the callback.
   *
   * @param callback
   *        Callback that gets called with the tab being moved as "this" and
   *        the event as the first argument.
   * @usage Tabs.onMove(handleMove);
   *        Tabs.onMove.unbind(handleMove);
   */
  get onMove() T.makeBind("onMove"),

  /**
   * Attach a callback for when a tab is opened.
   *
   * There is also an unbind method off of this method to remove the callback.
   *
   * @param callback
   *        Callback that gets called with the tab being opened as "this" and
   *        the event as the first argument.
   * @usage Tabs.onOpen(handleOpen);
   *        Tabs.onOpen.unbind(handleOpen);
   */
  get onOpen() T.makeBind("onOpen"),

  /**
   * Attach a callback for when a tab is selected.
   *
   * There is also an unbind method off of this method to remove the callback.
   *
   * @param callback
   *        Callback that gets called with the tab being selected as "this" and
   *        the event as the first argument.
   * @usage Tabs.onSelect(handleSelect);
   *        Tabs.onSelect.unbind(handleSelect);
   */
  get onSelect() T.makeBind("onSelect"),

  //////////////////////////////////////////////////////////////////////////////
  //// Private
  //////////////////////////////////////////////////////////////////////////////

  init: function init() {
    // Only allow calling init once
    T.init = function() T;

    // Register listeners on all browsers and future browsers
    T.allBrowsers.forEach(T.registerBrowser);
    Services.obs.addObserver(T, "domwindowopened", false);

    return T;
  },

  get allBrowsers() {
    let browsers = [];
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements())
      browsers.push(windows.getNext());
    return browsers;
  },

  eventMap: {
    TabAttrModified: "onChange",
    TabClose: "onClose",
    TabMove: "onMove",
    TabOpen: "onOpen",
    TabSelect: "onSelect",
  },

  registerBrowser: function registerBrowser(browser) {
    // Add a listener for each tab even to trigger the matching topic
    [i for (i in Iterator(T.eventMap))].forEach(function([tabEvent, topic]) {
      browser.addEventListener(tabEvent, function(event) {
        T.trigger(topic, event.originalTarget, event);
      }, true);
    });
  },

  listeners: {},

  makeBind: function makeBind(topic) {
    delete T[topic];
    T.listeners[topic] = [];

    // Allow adding listeners to this topic
    T[topic] = function bind(callback) {
      T.listeners[topic].push(callback);
    };

    // Allow removing listeners from this topic
    T[topic].unbind = function unbind(callback) {
      let index = T.listeners[topic].indexOf(callback);
      if (index != -1)
        T.listeners[topic].splice(index, 1);
    };

    return T[topic];
  },

  trigger: function trigger(topic, tab, event) {
    // Make sure we've gotten listeners before trying to call
    let listeners = T.listeners[topic];
    if (listeners == null)
      return;

    // Make a copy of the listeners, so it can't change as we call back
    listeners.slice().forEach(function(callback) {
      try {
        callback.call(tab, event);
      }
      // Ignore failures from the callback
      catch(ex) {}
    });
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver
  //////////////////////////////////////////////////////////////////////////////

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "domwindowopened":
        subject.addEventListener("load", function() {
          subject.removeEventListener("load", arguments.callee, false);

          // Now that the window has loaded, only register on browser windows
          let doc = subject.document.documentElement;
          if (doc.getAttribute("windowtype") == "navigator:browser")
            T.registerBrowser(subject);
        }, false);
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports
  //////////////////////////////////////////////////////////////////////////////

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
}) T.init();
