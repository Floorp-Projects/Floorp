/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function initFeedTab(feeds) {
  for (let feed of feeds) {
    let [name, type, url] = feed;
    addRow(name, type, url);
  }

  var feedListbox = document.getElementById("feedListbox");
  document.getElementById("feedTab").hidden = feedListbox.getRowCount() == 0;
}

function onSubscribeFeed() {
  var listbox = document.getElementById("feedListbox");
  openUILinkIn(listbox.selectedItem.getAttribute("feedURL"), "current",
               { ignoreAlt: true });
}

function addRow(name, type, url) {
  var item = document.createElement("richlistitem");
  item.setAttribute("feed", "true");
  item.setAttribute("name", name);
  item.setAttribute("type", type);
  item.setAttribute("feedURL", url);
  document.getElementById("feedListbox").appendChild(item);
}
