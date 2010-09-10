# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Tab View
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Raymond Lee <raymond@appcoast.com>
#   Ian Gilman <ian@iangilman.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

let TabView = {
  _deck: null,
  _window: null,
  _sessionstore: null,
  _visibilityID: "tabview-visibility",
  
  // ----------
  get windowTitle() {
    delete this.windowTitle;
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let title = gNavigatorBundle.getFormattedString("tabView2.title", [brandShortName]);
    return this.windowTitle = title;
  },

  // ----------
  init: function TabView_init() {    
    // ___ keys    
    this._setBrowserKeyHandlers();

    // ___ visibility
    this._sessionstore =
      Cc["@mozilla.org/browser/sessionstore;1"].
        getService(Ci.nsISessionStore);

    let data = this._sessionstore.getWindowValue(window, this._visibilityID);
    if (data && data == "true")
      this.show();
  },

  // ----------
  // Creates the frame and calls the callback once it's loaded. 
  // If the frame already exists, calls the callback immediately. 
  _initFrame: function TabView__initFrame(callback) {
    if (this._window) {
      if (typeof callback == "function")
        callback();
    } else {
      // ___ find the deck
      this._deck = document.getElementById("tab-view-deck");
      
      // ___ create the frame
      let iframe = document.createElement("iframe");
      iframe.id = "tab-view";
      iframe.setAttribute("transparent", "true");
      iframe.flex = 1;
              
      if (typeof callback == "function")
        iframe.addEventListener("DOMContentLoaded", callback, false);
      
      iframe.setAttribute("src", "chrome://browser/content/tabview.html");
      this._deck.appendChild(iframe);
      this._window = iframe.contentWindow;

      // ___ visibility storage handler
      let self = this;
      function observer(subject, topic, data) {
        if (topic == "quit-application-requested") {
          let data = (self.isVisible() ? "true" : "false");
          self._sessionstore.setWindowValue(window, self._visibilityID, data);
        }
      }
      
      Services.obs.addObserver(observer, "quit-application-requested", false);
    }
  },

  // ----------
  isVisible: function() {
    return (this._deck ? this._deck.selectedIndex == 1 : false);
  },

  // ----------
  show: function() {
    if (this.isVisible())
      return;
    
    this._initFrame(function() {
      let event = document.createEvent("Events");
      event.initEvent("tabviewshow", false, false);
      dispatchEvent(event);
    });
  },

  // ----------
  hide: function() {
    if (!this.isVisible())
      return;

    let event = document.createEvent("Events");
    event.initEvent("tabviewhide", false, false);
    dispatchEvent(event);
  },

  // ----------
  toggle: function() {
    if (this.isVisible())
      this.hide();
    else 
      this.show();
  },

  // ----------
  updateContextMenu: function(tab, popup) {
    let separator = document.getElementById("context_tabViewNamedGroups");
    let isEmpty = true;

    while (popup.firstChild && popup.firstChild != separator)
      popup.removeChild(popup.firstChild);

    let self = this;
    this._initFrame(function() {
      let activeGroup = tab.tabItem.parent;
      let groupItems = self._window.GroupItems.groupItems;

      groupItems.forEach(function(groupItem) {
        // if group has title, it's not hidden and there is no active group or
        // the active group id doesn't match the group id, a group menu item
        // would be added.
        if (groupItem.getTitle().length > 0 && !groupItem.hidden &&
            (!activeGroup || activeGroup.id != groupItem.id)) {
          let menuItem = self._createGroupMenuItem(groupItem);
          popup.insertBefore(menuItem, separator);
          isEmpty = false;
        }
      });
      separator.hidden = isEmpty;
    });
  },

  // ----------
  _createGroupMenuItem : function(groupItem) {
    let menuItem = document.createElement("menuitem")
    menuItem.setAttribute("label", groupItem.getTitle());
    menuItem.setAttribute(
      "oncommand", 
      "TabView.moveTabTo(TabContextMenu.contextTab,'" + groupItem.id + "')");

    return menuItem;
  },

  // ----------
  moveTabTo: function(tab, groupItemId) {
    if (this._window)
      this._window.GroupItems.moveTabToGroupItem(tab, groupItemId);
  },

  // ----------
  // Adds new key commands to the browser, for invoking the Tab Candy UI
  // and for switching between groups of tabs when outside of the Tab Candy UI.
  _setBrowserKeyHandlers : function() {
    let self = this;

    window.addEventListener("keypress", function(event) {
      if (self.isVisible())
        return;

      let charCode = event.charCode;
#ifdef XP_MACOSX
      // if a text box in a webpage has the focus, the event.altKey would
      // return false so we are depending on the charCode here.
      if (!event.ctrlKey && !event.metaKey && !event.shiftKey &&
          charCode == 160) { // alt + space
#else
      if (event.ctrlKey && !event.metaKey && !event.shiftKey && !event.altKey && 
          charCode == KeyEvent.DOM_VK_SPACE) { // ctrl + space
#endif

        // Don't handle this event if it's coming from a node that might allow
        // multiple keyboard selection like selects or trees
        let node = event.target;
        switch (node.namespaceURI) {
          case "http://www.w3.org/1999/xhtml":
            // xhtml:select only allows multiple when the attr is set
            if (node.localName == "select" && node.hasAttribute("multiple"))
              return;
            break;

          case "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul":
            switch (node.localName) {
              case "listbox":
                // xul:listbox is by default single
                if (node.getAttribute("seltype") == "multiple")
                  return;
                break;
              case "tree":
                // xul:tree is by default multiple
                if (node.getAttribute("seltype") != "single")
                  return;
                break;
            }
        }

        event.stopPropagation();
        event.preventDefault();
        self.show();
        return;
      }

      // Control (+ Shift) + `
      if (event.ctrlKey && !event.metaKey && !event.altKey &&
          (charCode == 96 || charCode == 126)) {
        event.stopPropagation();
        event.preventDefault();

        self._initFrame(function() {
          let tabItem = self._window.GroupItems.getNextGroupItemTab(event.shiftKey);
          if (tabItem)
            window.gBrowser.selectedTab = tabItem.tab;
        });
      }
    }, true);
  }
};
