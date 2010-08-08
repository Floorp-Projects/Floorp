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

// New API methods can be added to here.
let TabView = {
  init: function TabView_init() {
    var iframe = document.createElement("iframe");
    iframe.id = "tab-view";
    iframe.setAttribute("transparent", "true");
    iframe.flex = 1;
    iframe.setAttribute("src", "chrome://browser/content/tabview.html");
    document.getElementById("tab-view-deck").appendChild(iframe);
  },

  isVisible: function() {
    return (window.document.getElementById("tab-view-deck").selectedIndex == 1);
  },

  toggle: function() {
    let event = document.createEvent("Events");

    if (this.isVisible()) {
      event.initEvent("tabviewhide", false, false);
    } else {
      event.initEvent("tabviewshow", false, false);
    }
    dispatchEvent(event);
  },

  getWindowTitle: function() {
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    return gNavigatorBundle.getFormattedString("tabView.title", [brandShortName]);
  },

  updateContextMenu: function(tab, popup) {
    while(popup.lastChild && popup.lastChild.id != "context_namedGroups")
      popup.removeChild(popup.lastChild);

    let tabViewWindow = document.getElementById("tab-view").contentWindow;
    let isEmpty = true;

    if (tabViewWindow) {
      let activeGroup = tab.tabItem.parent;
      let groupItems = tabViewWindow.GroupItems.groupItems;
      let self = this;

      groupItems.forEach(function(groupItem) { 
        if (groupItem.getTitle().length > 0 && 
            (!activeGroup || activeGroup.id != groupItem.id)) {
          let menuItem = self._createGroupMenuItem(groupItem);
          popup.appendChild(menuItem);
          isEmpty = false;
        }
      });
    }
    document.getElementById("context_namedGroups").hidden = isEmpty;
  },

  _createGroupMenuItem : function(groupItem) {
    let menuItem = document.createElement("menuitem")
    menuItem.setAttribute("class", "group");
    menuItem.setAttribute("label", groupItem.getTitle());
    menuItem.setAttribute(
      "oncommand", 
      "TabView.moveTabTo(TabContextMenu.contextTab,'" + groupItem.id + "')");

    return menuItem;
  },

  moveTabTo: function(tab, groupItemId) {
    let tabViewWindow = document.getElementById("tab-view").contentWindow;

    if (tabViewWindow)
      tabViewWindow.GroupItems.moveTabToGroupItem(tab, groupItemId);
  }
};
