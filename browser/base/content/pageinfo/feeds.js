# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
# The Original Code is the feed tab for Page Info.
#
# The Initial Developer of the Original Code is
#   Florian QUEZE <f.qu@queze.net>
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

function initFeedTab()
{
  const feedTypes = {
    "application/rss+xml": gBundle.getString("feedRss"),
    "application/atom+xml": gBundle.getString("feedAtom")
  };

  // get the feeds
  var linkNodes = gDocument.getElementsByTagName("link");
  var length = linkNodes.length;
  for (var i = 0; i < length; i++) {
    if (linkNodes[i].rel == "alternate" &&
        linkNodes[i].type in feedTypes &&
        linkNodes[i].href) {
      addRow(linkNodes[i].title,
             feedTypes[linkNodes[i].type],
             linkNodes[i].href);
    }
  }

  var feedListbox = document.getElementById("feedListbox");
  if (feedListbox.getRowCount() > 0)
    document.getElementById("feedTab").hidden = false;
}

function onSubscribeFeed()
{
  var listbox = document.getElementById("feedListbox");
  openUILink(listbox.selectedItem.getAttribute("feedURL"),
             null, false, true, false, null);
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
