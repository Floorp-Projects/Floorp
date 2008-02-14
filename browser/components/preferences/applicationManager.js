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
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Florian Queze <florian@queze.net> (Original author)
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

#ifdef XP_MACOSX
var Cc = Components.classes;
var Ci = Components.interfaces;
#endif

var gAppManagerDialog = {
  _removed: [],

  init: function appManager_init() {
    this.handlerInfo = window.arguments[0];

    var bundle = document.getElementById("appManagerBundle");
    var contentText;
    if (this.handlerInfo.type == TYPE_MAYBE_FEED)
      contentText = bundle.getString("handleWebFeeds");
    else {
      var description = gApplicationsPane._describeType(this.handlerInfo);
      var key =
        (this.handlerInfo.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo) ? "handleFile"
                                                                        : "handleProtocol";
        contentText = bundle.getFormattedString(key, [description]);
    }
    contentText = bundle.getFormattedString("descriptionApplications", [contentText]);
    document.getElementById("appDescription").textContent = contentText;

    var list = document.getElementById("appList");
    var apps = this.handlerInfo.possibleApplicationHandlers.enumerate();
    while (apps.hasMoreElements()) {
      let app = apps.getNext();
      if (!gApplicationsPane.isValidHandlerApp(app))
        continue;

      app.QueryInterface(Ci.nsIHandlerApp);
      var item = list.appendItem(app.name);
      item.setAttribute("image", gApplicationsPane._getIconURLForHandlerApp(app));
      item.className = "listitem-iconic";
      item.app = app;
    }

    list.selectedIndex = 0;
  },

  onOK: function appManager_onOK() {
    if (!this._removed.length) {
      // return early to avoid calling the |store| method.
      return;
    }

    for (var i = 0; i < this._removed.length; ++i)
      this.handlerInfo.removePossibleApplicationHandler(this._removed[i]);

    this.handlerInfo.store();
  },

  onCancel: function appManager_onCancel() {
    // do nothing
  },

  remove: function appManager_remove() {
    var list = document.getElementById("appList");
    this._removed.push(list.selectedItem.app);
    var index = list.selectedIndex;
    list.removeItemAt(index);
    if (list.getRowCount() == 0) {
      // The list is now empty, make the bottom part disappear
      document.getElementById("appDetails").hidden = true;
    }
    else {
      // Select the item at the same index, if we removed the last
      // item of the list, select the previous item
      if (index == list.getRowCount())
        --index;
      list.selectedIndex = index;
    }
  },

  onSelect: function appManager_onSelect() {
    var list = document.getElementById("appList");
    if (!list.selectedItem) {
      document.getElementById("remove").disabled = true;
      return;
    }
    document.getElementById("remove").disabled = false;
    var app = list.selectedItem.app;
    var address = "";
    if (app instanceof Ci.nsILocalHandlerApp)
      address = app.executable.path;
    else if (app instanceof Ci.nsIWebHandlerApp)
      address = app.uriTemplate;
    else if (app instanceof Ci.nsIWebContentHandlerInfo)
      address = app.uri;
    document.getElementById("appLocation").value = address;
    var bundle = document.getElementById("appManagerBundle");
    var appType = app instanceof Ci.nsILocalHandlerApp ? "descriptionLocalApp"
                                                       : "descriptionWebApp";
    document.getElementById("appType").value = bundle.getString(appType);
  }
};
