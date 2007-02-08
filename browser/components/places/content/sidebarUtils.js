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
#   Dan Mills <thunder@mozilla.com> (Ported from history-panel.js)
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

var SidebarUtils = {
  handleClick: function SU_handleClick(aTree, aEvent, aGutterSelect) {
    var tbo = aTree.treeBoxObject;

    var row = { }, col = { }, obj = { };
    tbo.getCellAt(aEvent.clientX, aEvent.clientY, row, col, obj);

    if (row.value == -1 || obj.value == "twisty")
      return;

    var mouseInGutter = false;
    if (aGutterSelect) {
      var x = { }, y = { }, w = { }, h = { };
      tbo.getCoordsForCellItem(row.value, col.value, "image",
                               x, y, w, h);
      mouseInGutter = aEvent.clientX < x.value;
    }
    
    var modifKey = aEvent.shiftKey || aEvent.ctrlKey || aEvent.altKey || 
                   aEvent.metaKey  || (aEvent.button != 0);
    if (!modifKey && tbo.view.isContainer(row.value)) {
      tbo.view.toggleOpenState(row.value);
      return;
    }
    if (!mouseInGutter && 
        aEvent.originalTarget.localName == "treechildren" && 
        (aEvent.button == 0 || aEvent.button == 1)) {
      // Clear all other selection since we're loading a link now. We must
      // do this *before* attempting to load the link since openURL uses
      // selection as an indication of which link to load. 
      tbo.view.selection.select(row.value);
      aTree.controller.openSelectedNodeWithEvent(aEvent);
    }
  }
};
