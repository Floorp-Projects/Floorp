# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
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

function Startup()
{
  var extList = document.getElementById("extList");
  for (var i = 0; i < extList.childNodes.length; ++i) {
    if (extList.childNodes[i].getAttribute("name")) {
      extList.selectItem(extList.childNodes[i]);
      break;
    }
  }
}

function extensionSelect()
{
  var list = document.getElementById("extList");

  if (!list)
    return;

  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem) {
    var extName = selectedItem.getAttribute("displayName");
    var nameField = document.getElementById("extDisplayName");
    var author = document.getElementById("extAuthor");
    var descText = document.createTextNode(selectedItem.getAttribute("description"));
    var description = document.getElementById("extDescription");
    var uninstallButton = document.getElementById("uninstallExtension");
    var settingsButton = document.getElementById("extensionSettings");
    
    while (description.hasChildNodes())
      description.removeChild(description.firstChild);

    nameField.setAttribute("value", extName);
    
    author.setAttribute("value", selectedItem.getAttribute("author"));
    var authorURL = selectedItem.getAttribute("authorURL");
    if (authorURL != "") {
      author.setAttribute("link", selectedItem.getAttribute("authorURL"));
      author.className = "themesLink";
    }
    else {
      author.removeAttribute("link");
      author.className = "";
    }
    
    settingsButton.disabled = selectedItem.getAttribute("settingsURL") == "";
    
    description.appendChild(descText);

    updateDisableExtButton(selectedItem);
  }
}

function toggleExtension()
{
  var list = document.getElementById("extList");

  if (!list)
    return;

  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem) {
    var disabled = (selectedItem.getAttribute("disabledState") == "true");
    chromeRegistry.setAllowOverlaysForPackage(selectedItem.getAttribute("name"), disabled);
    updateDisableExtButton(selectedItem);
  }   
}

function updateDisableExtButton(item)
{
  var disableButton = document.getElementById("disableExtension");
  if (disableButton.disabled)
    disableButton.disabled = false;

  if (item.getAttribute("disabledState") == "true")
    disableButton.setAttribute("label", "Enable Extension"); // XXXdwh localize
  else
    disableButton.setAttribute("label", "Disable Extension"); // XXXdwh localize
}

function showSettings()
{
  var list = document.getElementById("extList");
  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem)
    window.openDialog(selectedItem.getAttribute("settingsURL"), "", "chrome,dialog,modal");
}

function visitLink (aEvent, aAuthor)
{
  var msg = "";
  if (aAuthor)
    msg = "prefCloseThemeAuthorLinkMsg";
  else
    msg = "prefCloseThemeNewExtensionLinkMsg";

  var node = aEvent.target;
  while (node.nodeType != Node.ELEMENT_NODE)
    node = node.parentNode;

  var url = node.getAttribute("link");
  if (url != "")
    parent.visitLink("prefCloseThemeLinkTitle", msg, url);
}
