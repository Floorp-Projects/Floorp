# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
