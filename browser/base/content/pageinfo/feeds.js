# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

function initFeedTab()
{
  const feedTypes = {
    "application/rss+xml": gBundle.getString("feedRss"),
    "application/atom+xml": gBundle.getString("feedAtom"),
    "text/xml": gBundle.getString("feedXML"),
    "application/xml": gBundle.getString("feedXML"),
    "application/rdf+xml": gBundle.getString("feedXML")
  };

  // get the feeds
  var linkNodes = gDocument.getElementsByTagName("link");
  var length = linkNodes.length;
  for (var i = 0; i < length; i++) {
    var link = linkNodes[i];
    if (!link.href)
      continue;

    var rel = link.rel && link.rel.toLowerCase();
    var rels = {};
    if (rel) {
      for each (let relVal in rel.split(/\s+/))
        rels[relVal] = true;
    }

    if (rels.feed || (link.type && rels.alternate && !rels.stylesheet)) {
      var type = isValidFeed(link, gDocument.nodePrincipal, rels.feed);
      if (type) {
        type = feedTypes[type] || feedTypes["application/rss+xml"];
        addRow(link.title, type, link.href);
      }
    }
  }

  var feedListbox = document.getElementById("feedListbox");
  document.getElementById("feedTab").hidden = feedListbox.getRowCount() == 0;
}

function onSubscribeFeed()
{
  var listbox = document.getElementById("feedListbox");
  openUILinkIn(listbox.selectedItem.getAttribute("feedURL"), "current",
               { ignoreAlt: true });
}

function addRow(name, type, url)
{
  var item = document.createElement("richlistitem");
  item.setAttribute("feed", "true");
  item.setAttribute("name", name);
  item.setAttribute("type", type);
  item.setAttribute("feedURL", url);
  document.getElementById("feedListbox").appendChild(item);
}
