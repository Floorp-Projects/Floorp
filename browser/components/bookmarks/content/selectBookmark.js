# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Pierre Chanial <chanial@noos.fr> (Original Author)
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

var gBookmarkTree;
var gOK;
var gUrls;
var gNames;

function Startup()
{
  initServices();
  initBMService();
  gOK = document.documentElement.getButton("accept");
  gBookmarkTree = document.getElementById("bookmarks-view");  
  gBookmarkTree.treeBoxObject.view.selection.select(0);
  gBookmarkTree.focus();
}

function onDblClick()
{
  if (!gOK.disabled)
    document.documentElement.acceptDialog();
}

function updateOK()
{
  var selection = gBookmarkTree._selection;
  var ds = gBookmarkTree.tree.database;
  var url, name;
  gUrls = [];
  gNames = [];
  for (var i=0; i<selection.length; ++i) {
    var type     = selection.type[i];
// XXX protocol is broken since we have unique id...
//    var protocol = selection.protocol[i];
//    if ((type == "Bookmark" || type == "") && 
//        protocol != "find" && protocol != "javascript") {
    if (type == "Bookmark" || type == "") {
      url = BookmarksUtils.getProperty(selection.item[i], gNC_NS+"URL", ds);
      name = BookmarksUtils.getProperty(selection.item[i], gNC_NS+"Name", name);
      if (url && name) {
        gUrls.push(url);
        gNames.push(name);
      }
    } else if (type == "Folder" || type == "PersonalToolbarFolder") {
      RDFC.Init(ds, selection.item[i]);
      var children = RDFC.GetElements();
      while (children.hasMoreElements()) {
        var child = children.getNext().QueryInterface(kRDFRSCIID);
        type      = BookmarksUtils.getProperty(child, gRDF_NS+"type", ds);
// XXX protocol is broken since we have unique id...
//        protocol  = child.Value.split(":")[0];
//        if (type == gNC_NS+"Bookmark" && protocol != "find" && 
//            protocol != "javascript") {
          if (type == gNC_NS+"Bookmark") {
          url = BookmarksUtils.getProperty(child, gNC_NS+"URL", ds);
          name = BookmarksUtils.getProperty(child, gNC_NS+"Name", ds);
          if (url && name) {
            gUrls.push(url);
            gNames.push(name);
          }
        }
      }
    }
  }
  gOK.disabled = gUrls.length == 0;
}

function onOK(aEvent)
{
  window.arguments[0].urls = gUrls;
  window.arguments[0].names = gNames;
}
