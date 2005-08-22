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
# Blake Ross (blake@blakeross.com).
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Blake Ross <blake@blakeross.com>
#   Ben Goodger <ben@mozilla.org>
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
# ***** END LICENSE BLOCK *****

const kXUL_NS            = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kIShellService    = Components.interfaces.nsIShellService;
#ifdef XP_MACOSX
const kIMacShellService = Components.interfaces.nsIMacShellService;
#endif

var gSetBackground = {
  _position         : kIShellService.BACKGROUND_STRETCH,
  _monitor          : null,
  _image            : null,
  _backgroundColor  : 0,

#ifndef XP_MACOSX
  // Converts a color string in the format "#RRGGBB" to an integer.
  _hexStringToLong: function (aString)
  {
    return parseInt(aString.substring(1,3), 16) << 16 | 
           parseInt(aString.substring(3,5), 16) << 8 |
           parseInt(aString.substring(5,7), 16);
  },
  
  _rgbToHex: function(aR, aG, aB) 
  {
    var rHex = aR.toString(16).toUpperCase();
    var gHex = aG.toString(16).toUpperCase();
    var bHex = aB.toString(16).toUpperCase();

    if (rHex.length == 1) rHex ='0' + rHex;
    if (gHex.length == 1) gHex ='0' + gHex;
    if (bHex.length == 1) bHex ='0' + bHex;

    return '#' + rHex + gHex + bHex;
  },
#endif

  get _shell()
  {
    return Components.classes["@mozilla.org/browser/shell-service;1"]
                     .getService(Components.interfaces.nsIShellService);
  },

  load: function ()
  {
    this._monitor = document.getElementById("monitor");
#ifdef XP_MACOSX
    document.documentElement.getButton("accept").hidden = true;
#endif
    this.init(window.arguments[0]);
  },
        
  init: function (aImage)
  {
    this._image = aImage;
 
#ifndef XP_MACOSX
    this._initColor();
    var position = parseInt(document.getElementById("menuPosition").value);
#else
    // Make sure to reset the button state in case the user has already
    // set an image as their desktop background. 
    var setDesktopBackground = document.getElementById("setDesktopBackground");
    setDesktopBackground.hidden = false;
    var bundle = document.getElementById("backgroundBundle");
    setDesktopBackground.label = bundle.getString("DesktopBackgroundSet");
    setDesktopBackground.disabled = false;
    
    var showDesktopPreferences = document.getElementById("showDesktopPreferences");
    showDesktopPreferences.hidden = true;

    var position = kIShellService.BACKGROUND_STRETCH;
#endif
    this.updatePosition(position);
  },
        
#ifndef XP_MACOSX
  _initColor: function ()
  {
    var color = this._shell.desktopBackgroundColor;

    const rMask = 4294901760;
    const gMask = 65280;
    const bMask = 255;
    var r = (color & rMask) >> 16;
    var g = (color & gMask) >> 8;
    var b = (color & bMask);
    this._backgroundColor = this._rgbToHex(r, g, b);

    var colorpicker = document.getElementById("desktopColor");
    colorpicker.color = this._rgbToHex(r, g, b);
  },
#endif

  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "shell:desktop-background-changed") {
      var setDesktopBackground = document.getElementById("setDesktopBackground");
      setDesktopBackground.hidden = true;
      
      var showDesktopPreferences = document.getElementById("showDesktopPreferences");
      showDesktopPreferences.hidden = false;

      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Components.interfaces.nsIObserverService);
      os.removeObserver(this, "shell:desktop-background-changed");
    }
  },

#ifdef XP_MACOSX
  setDesktopBackground: function()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "shell:desktop-background-changed", false);
    
    var bundle = document.getElementById("backgroundBundle");
    var setDesktopBackground = document.getElementById("setDesktopBackground");
    setDesktopBackground.disabled = true;
    setDesktopBackground.label = bundle.getString("DesktopBackgroundDownloading");

    this._shell.setDesktopBackground(this._image, this._position);
  },
  
  showDesktopPrefs: function()
  {
    this._shell.openApplication(kIMacShellService.APPLICATION_DESKTOP);
  },
#else
  setDesktopBackground: function () 
  {
    this._shell.setDesktopBackground(this._image, this._position);
    this._shell.desktopBackgroundColor = this._hexStringToLong(this._backgroundColor);
    document.persist("menuPosition", "value");
  },
#endif

  updateColor: function (color)
  {
#ifndef XP_MACOSX
    this._backgroundColor = color;
    
    if (this._position != kIShellService.BACKGROUND_TILE)
      this._monitor.style.backgroundColor = color;
#endif
  },
  
  updatePosition: function (aPosition)
  {
    if (this._monitor.childNodes.length)
      this._monitor.removeChild(this._monitor.firstChild);
      
    this._position = aPosition;
    if (this._position == kIShellService.BACKGROUND_TILE)
      this._tileImage();
    else if (this._position == kIShellService.BACKGROUND_STRETCH)
      this._stretchImage();
    else
      this._centerImage();
  },

  _createImage: function ()
  {
    const nsIImageLoadingContent = Components.interfaces.nsIImageLoadingContent;
    if (!(this._image instanceof nsIImageLoadingContent))
        return false;

    var request = this._image.QueryInterface(nsIImageLoadingContent)
                             .getRequest(nsIImageLoadingContent.CURRENT_REQUEST);
    if (!request)
      return false;

    var imgURI = this._image.currentURI;
    if (imgURI.schemeIs("javascript"))
      return false;

    var img = document.createElementNS(kXUL_NS, "image");
    img.setAttribute("src", imgURI.spec);
    return img;
  },
        
  _stretchImage: function ()
  {  
    this.updateColor(this._backgroundColor);

    var img = this._createImage();
    img.width = parseInt(this._monitor.style.width);
    img.height = parseInt(this._monitor.style.height);
    this._monitor.appendChild(img);
  },
        
  _tileImage: function ()
  {
    var bundle = document.getElementById("backgroundBundle");

    this._monitor.style.backgroundColor = "white";

    var text = document.createElementNS(kXUL_NS, "label");
    text.setAttribute("id", "noPreviewAvailable");
    text.setAttribute("value", bundle.getString("DesktopBackgroundNoPreview"));
    this._monitor.appendChild(text);
  },
        
  _centerImage: function ()
  {
    this.updateColor(this._backgroundColor);
             
    var img = this._createImage();
    var width = this._image.width * this._monitor.boxObject.width / screen.width;
    var height = this._image.height * this._monitor.boxObject.height / screen.height;
    img.width = Math.floor(width);
    img.height = Math.floor(height);
    this._monitor.appendChild(img);
  }
};
