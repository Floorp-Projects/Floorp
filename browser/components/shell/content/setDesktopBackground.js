/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../base/content/utilityOverlay.js */

var gSetBackground = {
  _position: AppConstants.platform == "macosx" ? "STRETCH" : "",
  _backgroundColor: AppConstants.platform != "macosx" ? 0 : undefined,
  _screenWidth: 0,
  _screenHeight: 0,
  _image: null,
  _canvas: null,
  _imageName: null,

  get _shell() {
    return Cc["@mozilla.org/browser/shell-service;1"]
             .getService(Ci.nsIShellService);
  },

  load() {
    this._canvas = document.getElementById("screen");
    this._screenWidth = screen.width;
    this._screenHeight = screen.height;
    // Cap ratio to 4 so the dialog width doesn't get ridiculous. Highest
    // regular screens seem to be 32:9 (3.56) according to Wikipedia.
    let screenRatio = Math.min(this._screenWidth / this._screenHeight, 4);
    this._canvas.width = this._canvas.height * screenRatio;
    document.getElementById("preview-unavailable").style.width =
      this._canvas.width + "px";

    if (AppConstants.platform == "macosx") {
      document.documentElement.getButton("accept").hidden = true;
    } else {
      let multiMonitors = false;
      if (AppConstants.platform == "linux") {
        // getMonitors only ever returns the primary monitor on Linux, so just
        // always show the option
        multiMonitors = true;
      } else {
        const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
        const monitors = gfxInfo.getMonitors();
        multiMonitors = monitors.length > 1;
      }

      if (!multiMonitors || AppConstants.isPlatformAndVersionAtMost("win", 6.1)) {
        // Hide span option if < Win8 since that's when it was introduced.
        document.getElementById("spanPosition").hidden = true;
      }
    }

    document.addEventListener("dialogaccept", function() { gSetBackground.setDesktopBackground(); });
    // make sure that the correct dimensions will be used
    setTimeout(function(self) {
      self.init(window.arguments[0], window.arguments[1]);
    }, 0, this);
  },

  init(aImage, aImageName) {
    this._image = aImage;
    this._imageName = aImageName;

    // set the size of the coordinate space
    this._canvas.width = this._canvas.clientWidth;
    this._canvas.height = this._canvas.clientHeight;

    var ctx = this._canvas.getContext("2d");
    ctx.scale(this._canvas.clientWidth / this._screenWidth, this._canvas.clientHeight / this._screenHeight);

    if (AppConstants.platform != "macosx") {
      this._initColor();
    } else {
      // Make sure to reset the button state in case the user has already
      // set an image as their desktop background.
      var setDesktopBackground = document.getElementById("setDesktopBackground");
      setDesktopBackground.hidden = false;
      var bundle = document.getElementById("backgroundBundle");
      setDesktopBackground.label = bundle.getString("DesktopBackgroundSet");
      setDesktopBackground.disabled = false;

      document.getElementById("showDesktopPreferences").hidden = true;
    }
    this.updatePosition();
  },

  setDesktopBackground() {
    if (AppConstants.platform != "macosx") {
      Services.xulStore.persist(document.getElementById("menuPosition"), "value");
      this._shell.desktopBackgroundColor = this._hexStringToLong(this._backgroundColor);
    } else {
      Services.obs.addObserver(this, "shell:desktop-background-changed");

      var bundle = document.getElementById("backgroundBundle");
      var setDesktopBackground = document.getElementById("setDesktopBackground");
      setDesktopBackground.disabled = true;
      setDesktopBackground.label = bundle.getString("DesktopBackgroundDownloading");
    }
    this._shell.setDesktopBackground(
      this._image,
      Ci.nsIShellService["BACKGROUND_" + this._position],
      this._imageName);
  },

  updatePosition() {
    var ctx = this._canvas.getContext("2d");
    ctx.clearRect(0, 0, this._screenWidth, this._screenHeight);
    document.getElementById("preview-unavailable").hidden = true;

    if (AppConstants.platform != "macosx") {
      this._position = document.getElementById("menuPosition").value;
    }

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
      case "CENTER": {
        let x = (this._screenWidth - this._image.naturalWidth) / 2;
        let y = (this._screenHeight - this._image.naturalHeight) / 2;
        ctx.drawImage(this._image, x, y);
        break;
      }
      case "FILL": {
        // Try maxing width first, overflow height.
        let widthRatio = this._screenWidth / this._image.naturalWidth;
        let width = this._image.naturalWidth * widthRatio;
        let height = this._image.naturalHeight * widthRatio;
        if (height < this._screenHeight) {
          // Height less than screen, max height and overflow width.
          let heightRatio = this._screenHeight / this._image.naturalHeight;
          width = this._image.naturalWidth * heightRatio;
          height = this._image.naturalHeight * heightRatio;
        }
        let x = (this._screenWidth - width) / 2;
        let y = (this._screenHeight - height) / 2;
        ctx.drawImage(this._image, x, y, width, height);
        break;
      }
      case "FIT": {
        // Try maxing width first, top and bottom borders.
        let widthRatio = this._screenWidth / this._image.naturalWidth;
        let width = this._image.naturalWidth * widthRatio;
        let height = this._image.naturalHeight * widthRatio;
        let x = 0;
        let y = (this._screenHeight - height) / 2;
        if (height > this._screenHeight) {
          // Height overflow, maximise height, side borders.
          let heightRatio = this._screenHeight / this._image.naturalHeight;
          width = this._image.naturalWidth * heightRatio;
          height = this._image.naturalHeight * heightRatio;
          x = (this._screenWidth - width) / 2;
          y = 0;
        }
        ctx.drawImage(this._image, x, y, width, height);
        break;
      }
      case "SPAN": {
        document.getElementById("preview-unavailable").hidden = false;
        ctx.fillStyle = "#222";
        ctx.fillRect(0, 0, this._screenWidth, this._screenHeight);
        ctx.stroke();
      }
    }
  },
};

if (AppConstants.platform != "macosx") {
  gSetBackground._initColor = function() {
    var color = this._shell.desktopBackgroundColor;

    const rMask = 4294901760;
    const gMask = 65280;
    const bMask = 255;
    var r = (color & rMask) >> 16;
    var g = (color & gMask) >> 8;
    var b = (color & bMask);
    this.updateColor(this._rgbToHex(r, g, b));

    var colorpicker = document.getElementById("desktopColor");
    colorpicker.value = this._backgroundColor;
  };

  gSetBackground.updateColor = function(aColor) {
    this._backgroundColor = aColor;
    this._canvas.style.backgroundColor = aColor;
  };

  // Converts a color string in the format "#RRGGBB" to an integer.
  gSetBackground._hexStringToLong = function(aString) {
    return parseInt(aString.substring(1, 3), 16) << 16 |
           parseInt(aString.substring(3, 5), 16) << 8 |
           parseInt(aString.substring(5, 7), 16);
  };

  gSetBackground._rgbToHex = function(aR, aG, aB) {
    return "#" + [aR, aG, aB].map(aInt => aInt.toString(16).replace(/^(.)$/, "0$1"))
                             .join("").toUpperCase();
  };
} else {
  gSetBackground.observe = function(aSubject, aTopic, aData) {
    if (aTopic == "shell:desktop-background-changed") {
      document.getElementById("setDesktopBackground").hidden = true;
      document.getElementById("showDesktopPreferences").hidden = false;

      Services.obs.removeObserver(this, "shell:desktop-background-changed");
    }
  };

  gSetBackground.showDesktopPrefs = function() {
    this._shell.openApplication(Ci.nsIMacShellService.APPLICATION_DESKTOP);
  };
}
