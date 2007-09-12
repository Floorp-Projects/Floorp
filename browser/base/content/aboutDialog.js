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
# The Original Code is Mozilla Firebird about dialog.
#
# The Initial Developer of the Original Code is
# Blake Ross (blaker@netscape.com).
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

var gSelectedPage = 0;

function init(aEvent) 
{
  if (aEvent.target != document)
    return;

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

  try {
    var distroId = prefs.getCharPref("distribution.id");
    if (distroId) {
      var distroVersion = prefs.getCharPref("distribution.version");
      var distroAbout = prefs.getComplexValue("distribution.about", Ci.nsISupportsString);
  
      var distroField = document.getElementById("distribution");
      distroField.value = distroAbout;
      distroField.style.display = "block";
    
      var distroIdField = document.getElementById("distributionId");
      distroIdField.value = distroId + " - " + distroVersion;
      distroIdField.style.display = "block";
    }
  }
  catch (e) {
    // Pref is unset
  }

  var userAgentField = document.getElementById("userAgent");
  userAgentField.value = navigator.userAgent;

  var button = document.documentElement.getButton("extra2");
  button.setAttribute("label", document.documentElement.getAttribute("creditslabel"));
  button.setAttribute("accesskey", document.documentElement.getAttribute("creditsaccesskey"));
  button.addEventListener("command", switchPage, false);

  document.documentElement.getButton("accept").focus();
#ifdef XP_MACOSX
  // it may not be sized at this point, and we need its width to calculate its position
  window.sizeToContent();
  window.moveTo((screen.availWidth / 2) - (window.outerWidth / 2), screen.availHeight / 5);
#endif
}

function uninit(aEvent)
{
  if (aEvent.target != document)
    return;
  var iframe = document.getElementById("creditsIframe");
  iframe.setAttribute("src", "");
}

function switchPage(aEvent)
{
  var button = aEvent.target;
  if (button.localName != "button")
    return;

  var iframe = document.getElementById("creditsIframe");
  if (gSelectedPage == 0) { 
    iframe.setAttribute("src", "chrome://browser/content/credits.xhtml");
    button.setAttribute("label", document.documentElement.getAttribute("aboutlabel"));
    button.setAttribute("accesskey", document.documentElement.getAttribute("aboutaccesskey"));
    gSelectedPage = 1;
  }
  else {
    iframe.setAttribute("src", ""); 
    button.setAttribute("label", document.documentElement.getAttribute("creditslabel"));
    button.setAttribute("accesskey", document.documentElement.getAttribute("creditsaccesskey"));
    gSelectedPage = 0;
  }
  var modes = document.getElementById("modes");
  modes.setAttribute("selectedIndex", gSelectedPage);
}

