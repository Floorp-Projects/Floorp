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
#   Dão Gottwald <dao@design-noir.de>
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

var Ci = Components.interfaces;

var gSetBackground = {
#ifndef XP_MACOSX
  _position        : "",
  _backgroundColor : 0,
#else
  _position        : "STRETCH",
#endif
  _screenWidth     : 0,
  _screenHeight    : 0,
  _image           : null,
  _canvas          : null,

  get _shell()
  {
    return Components.classes["@mozilla.org/browser/shell-service;1"]
                     .getService(Ci.nsIShellService);
  },

  load: function ()
  {
    this._canvas = document.getElementById("screen");
    this._screenWidth = screen.width;
    this._screenHeight = screen.height;
#ifdef XP_MACOSX
    document.documentElement.getButton("accept").hidden = true;
#endif
    if (this._screenWidth / this._screenHeight >= 1.6)
      document.getElementById("monitor").setAttribute("aspectratio", "16:10");

    // make sure that the correct dimensions will be used
    setTimeout(function(self) {
      self.init(window.arguments[0]);
    }, 0, this);
  },

  init: function (aImage)
  {
    this._image = aImage;

    // set the size of the coordinate space
    this._canvas.width = this._canvas.clientWidth;
    this._canvas.height = this._canvas.clientHeight;

    var ctx = this._canvas.getContext("2d");
    ctx.scale(this._canvas.clientWidth / this._screenWidth, this._canvas.clientHeight / this._screenHeight);

#ifndef XP_MACOSX
    this._initColor();
#else
    // Make sure to reset the button state in case the user has already
    // set an image as their desktop background.
    var setDesktopBackground = document.getElementById("setDesktopBackground");
    setDesktopBackground.hidden = false;
    var bundle = document.getElementById("backgroundBundle");
    setDesktopBackground.label = bundle.getString("DesktopBackgroundSet");
    setDesktopBackground.disabled = false;

    document.getElementById("showDesktopPreferences").hidden = true;
#endif
    this.updatePosition();
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
    this.updateColor(this._rgbToHex(r, g, b));

    var colorpicker = document.getElementById("desktopColor");
    colorpicker.color = this._backgroundColor;
  },

  updateColor: function (aColor)
  {
    this._backgroundColor = aColor;
    this._canvas.style.backgroundColor = aColor;
  },

  // Converts a color string in the format "#RRGGBB" to an integer.
  _hexStringToLong: function (aString)
  {
    return parseInt(aString.substring(1,3), 16) << 16 |
           parseInt(aString.substring(3,5), 16) << 8 |
           parseInt(aString.substring(5,7), 16);
  },

  _rgbToHex: function (aR, aG, aB)
  {
    return "#" + [aR, aG, aB].map(function(aInt) aInt.toString(16).replace(/^(.)$/, "0$1"))
                             .join("").toUpperCase();
  },
#else
  observe: function (aSubject, aTopic, aData)
  {
    if (aTopic == "shell:desktop-background-changed") {
      document.getElementById("setDesktopBackground").hidden = true;
      document.getElementById("showDesktopPreferences").hidden = false;

      Components.classes["@mozilla.org/observer-service;1"]
                .getService(Ci.nsIObserverService)
                .removeObserver(this, "shell:desktop-background-changed");
    }
  },

  showDesktopPrefs: function()
  {
    this._shell.openApplication(Ci.nsIMacShellService.APPLICATION_DESKTOP);
  },
#endif

  setDesktopBackground: function ()
  {
#ifndef XP_MACOSX
    document.persist("menuPosition", "value");
    this._shell.desktopBackgroundColor = this._hexStringToLong(this._backgroundColor);
#else
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService)
              .addObserver(this, "shell:desktop-background-changed", false);

    var bundle = document.getElementById("backgroundBundle");
    var setDesktopBackground = document.getElementById("setDesktopBackground");
    setDesktopBackground.disabled = true;
    setDesktopBackground.label = bundle.getString("DesktopBackgroundDownloading");
#endif
    this._shell.setDesktopBackground(this._image,
                                     Ci.nsIShellService["BACKGROUND_" + this._position]);
  },

  updatePosition: function ()
  {
    var ctx = this._canvas.getContext("2d");
    ctx.clearRect(0, 0, this._screenWidth, this._screenHeight);

#ifndef XP_MACOSX
    this._position = document.getElementById("menuPosition").value;
#endif

    switch (this._position) {
      case "TILE":
        ctx.save();
        ctx.fillStyle = ctx.createPattern(this._image, "repeat");
        ctx.fillRect(0, 0, this._screenWidth, this._screenHeight);
        ctx.restore();
        break;
      case "STRETCH":
        ctx.drawImage(this._image, 0, 0, this._screenWidth, this._screenHeight);
        break;
      case "CENTER":
        var x = (this._screenWidth - this._image.naturalWidth) / 2;
        var y = (this._screenHeight - this._image.naturalHeight) / 2;
        ctx.drawImage(this._image, x, y);
    }
  }
};
