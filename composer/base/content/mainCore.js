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
# The Original Code is Mozilla.org
#
# The Initial Developer of the Original Code is
# David Hyatt.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   David Hyatt (hyatt@apple.com)
#   Daniel Glazman (daniel@glazman.org), on behalf of Lindows.com
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

function CustomizeMainToolbar(id)
{
  // make sure our toolbar buttons have the correct enabled state restored to them...
  if (this.UpdateMainToolbar == undefined)
      dump("UPDATEMAINTOOLBAR undefined\n");

  // Disable the toolbar context menu items
  // var menubar = document.getElementById("main-menubar");
  // for (var i = 0; i < menubar.childNodes.length; ++i)
  //   menubar.childNodes[i].setAttribute("disabled", true);
    
  var customizePopup = document.getElementById("CustomizeMainToolbar");
  if (customizePopup)
    customizePopup.setAttribute("disabled", "true");
   
  window.openDialog("chrome://global/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById(id));
}

function MainToolboxCustomizeDone(aToolboxChanged)
{
  // Update global UI elements that may have been added or removed

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);

  var customizePopup = document.getElementById("CustomizeMainToolbar");
  if (customizePopup)
    customizePopup.removeAttribute("disabled");

  // make sure our toolbar buttons have the correct enabled state restored to them...
  if (this.UpdateMainToolbar != undefined)
    UpdateMainToolbar(focus); 
}

function onViewToolbarCommand(aToolbarId, aMenuItemId)
{
  var toolbar = document.getElementById(aToolbarId);
  var menuItem = document.getElementById(aMenuItemId);

  if (!toolbar || !menuItem) return;

  var toolbarCollapsed = toolbar.collapsed;

  // toggle the checkbox
  menuItem.setAttribute('checked', toolbarCollapsed);

  // toggle visibility of the toolbar
  toolbar.collapsed = !toolbarCollapsed;

  document.persist(aToolbarId, 'collapsed');
  document.persist(aMenuItemId, 'checked');
}

function UpdateMainToolbar(caller)
{
    dump("===> UPDATING TOOLBAR\n");
}
