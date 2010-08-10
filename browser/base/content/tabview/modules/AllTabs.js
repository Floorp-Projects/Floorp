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
 * The Original Code is TabView AllTabs.
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

let EXPORTED_SYMBOLS = ["AllTabs"];

let AllTabs = {
  /**
   * Get an array of all tabs from all tabbrowser windows.
   *
   * @usage let numAllTabs = AllTabs.tabs.length;
   *        AllTabs.tabs.forEach(handleAllTabs);
   */
  get tabs() {
    // Get tabs from each browser window and flatten them into one array
    return Array.concat.apply(null, browserWindows.map(function(browserWindow) {
      return Array.slice(browserWindow.gBrowser.tabs);
    }));
  },

  /**
   * Attach a callback for a given tab event.
   *
   * @param eventName
   *        Name of the corresponding Tab* Event; one of "attrModified",
   *        "close", "move", "open", "select".
   * @param callback
   *        Callback that gets called with the tab as the first argument and
   *        the event as the second argument.
   * @usage AllTabs.register("change", function handleChange(tab, event) {});
   */
  register: function register(eventName, callback) {
    // Either create the first entry or add additional callbacks
    let listeners = eventListeners[eventName];
    if (listeners == null)
      eventListeners[eventName] = [callback];
    else
      listeners.push(callback);
  },

  /**
   * Remove a callback for a given tab event.
   *
   * @param eventName
   *        Name of the corresponding Tab* Event; one of "attrModified",
   *        "close", "move", "open", "select".
   * @param callback
   *        The callback given for the original AllTabs.register call.
   * @usage AllTabs.unregister("close", handleClose);
   */
  unregister: function unregister(eventName, callback) {
    // Nothing to remove for this event
    let listeners = eventListeners[eventName];
    if (listeners == null)
      return;

    // Can only remove a callback if we have it
    let index = listeners.indexOf(callback);
    if (index == -1)
      return;

    listeners.splice(index, 1);
  }
};

__defineGetter__("browserWindows", function browserWindows() {
  let browserWindows = [];
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements())
    browserWindows.push(windows.getNext());
  return browserWindows;
});

let events = ["attrModified", "close", "move", "open", "select"];
let eventListeners = {};

function registerBrowserWindow(browserWindow) {
  events.forEach(function(eventName) {
    let tabEvent = "Tab" + eventName[0].toUpperCase() + eventName.slice(1);
    browserWindow.addEventListener(tabEvent, function(event) {
      // Make sure we've gotten listeners before trying to call
      let listeners = eventListeners[eventName];
      if (listeners == null)
        return;

      let tab = event.originalTarget;

      // Make a copy of the listeners, so it can't change as we call back
      listeners.slice().forEach(function(callback) {
        try {
          callback(tab, event);
        }
        // Ignore failures from the callback
        catch(ex) {}
      });
    }, true);
  });
}

let observer = {
  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "domwindowopened":
        subject.addEventListener("load", function() {
          subject.removeEventListener("load", arguments.callee, false);

          // Now that the window has loaded, only register on browser windows
          let doc = subject.document.documentElement;
          if (doc.getAttribute("windowtype") == "navigator:browser")
            registerBrowserWindow(subject);
        }, false);
        break;
    }
  }
};

// Register listeners on all browser windows and future ones
browserWindows.forEach(registerBrowserWindow);
Services.obs.addObserver(observer, "domwindowopened", false);
