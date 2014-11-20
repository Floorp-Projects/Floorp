/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var gSearchPane = {

  init: function ()
  {
    let list = document.getElementById("defaultEngine");
    let currentEngine = Services.search.currentEngine.name;
    Services.search.getVisibleEngines().forEach(e => {
      let item = list.appendItem(e.name);
      item.setAttribute("class", "menuitem-iconic searchengine-menuitem menuitem-with-favicon");
      if (e.iconURI)
        item.setAttribute("image", e.iconURI.spec);
      item.engine = e;
      if (e.name == currentEngine)
        list.selectedItem = item;
    });

    this.displayOneClickEnginesList();

    document.getElementById("oneClickProvidersList")
            .addEventListener("CheckboxStateChange", gSearchPane.saveOneClickEnginesList);
  },

  displayOneClickEnginesList: function () {
    let richlistbox = document.getElementById("oneClickProvidersList");
    let pref = document.getElementById("browser.search.hiddenOneOffs").value;
    let hiddenList = pref ? pref.split(",") : [];

    while (richlistbox.firstChild)
      richlistbox.firstChild.remove();

    let currentEngine = Services.search.currentEngine.name;
    Services.search.getVisibleEngines().forEach(e => {
      if (e.name == currentEngine)
        return;

      let item = document.createElement("richlistitem");
      item.setAttribute("label", e.name);
      if (hiddenList.indexOf(e.name) == -1)
        item.setAttribute("checked", "true");
      if (e.iconURI)
        item.setAttribute("src", e.iconURI.spec);
      richlistbox.appendChild(item);
    });
  },

  saveOneClickEnginesList: function () {
    let richlistbox = document.getElementById("oneClickProvidersList");
    let hiddenList = [];
    for (let child of richlistbox.childNodes) {
      if (!child.checked)
        hiddenList.push(child.getAttribute("label"));
    }
    document.getElementById("browser.search.hiddenOneOffs").value =
      hiddenList.join(",");
  },

  setDefaultEngine: function () {
    Services.search.currentEngine =
      document.getElementById("defaultEngine").selectedItem.engine;
    this.displayOneClickEnginesList();
  }
};
