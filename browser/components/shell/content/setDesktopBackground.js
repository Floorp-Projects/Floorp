# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

#ifdef XP_WIN
    // hide fill + fit options if <win7 since don't work 
    var version = Components.classes["@mozilla.org/system-info;1"]
                  .getService(Ci.nsIPropertyBag2)
                  .getProperty("version");
    var isWindows7OrHigher = (parseFloat(version) >= 6.1);
    if (!isWindows7OrHigher) {
      document.getElementById("fillPosition").hidden = true;
      document.getElementById("fitPosition").hidden = true;
    }
#endif

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
    return "#" + [aR, aG, aB].map(aInt => aInt.toString(16).replace(/^(.)$/, "0$1"))
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
        break;
      case "FILL":
        //Try maxing width first, overflow height
        var widthRatio = this._screenWidth / this._image.naturalWidth;
        var width = this._image.naturalWidth * widthRatio;
        var height = this._image.naturalHeight * widthRatio;
        if (height < this._screenHeight) {
          //height less than screen, max height and overflow width
          var heightRatio = this._screenHeight / this._image.naturalHeight;
          width = this._image.naturalWidth * heightRatio;
          height = this._image.naturalHeight * heightRatio;
        }
        var x = (this._screenWidth - width) / 2;
        var y = (this._screenHeight - height) / 2;
        ctx.drawImage(this._image, x, y, width, height);
        break;
      case "FIT":
        //Try maxing width first, top and bottom borders
        var widthRatio = this._screenWidth / this._image.naturalWidth;
        var width = this._image.naturalWidth * widthRatio;
        var height = this._image.naturalHeight * widthRatio;
        var x = 0;
        var y = (this._screenHeight - height) / 2;
        if (height > this._screenHeight) {
          //height overflow, maximise height, side borders
          var heightRatio = this._screenHeight / this._image.naturalHeight;
          width = this._image.naturalWidth * heightRatio;
          height = this._image.naturalHeight * heightRatio;
          x = (this._screenWidth - width) / 2;
          y = 0;
        }
        ctx.drawImage(this._image, x, y, width, height);
        break;      
    }
  }
};
