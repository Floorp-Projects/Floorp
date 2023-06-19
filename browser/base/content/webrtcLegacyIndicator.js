/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { webrtcUI } = ChromeUtils.importESModule(
  "resource:///modules/webrtcUI.sys.mjs"
);

function init(event) {
  for (let id of ["audioVideoButton", "screenSharePopup"]) {
    let popup = document.getElementById(id);
    popup.addEventListener("popupshowing", onPopupMenuShowing);
    popup.addEventListener("popuphiding", onPopupMenuHiding);
    popup.addEventListener("command", onPopupMenuCommand);
  }

  let fxButton = document.getElementById("firefoxButton");
  fxButton.addEventListener("click", onFirefoxButtonClick);
  fxButton.addEventListener("mousedown", PositionHandler);

  updateIndicatorState();

  // Alert accessibility implementations stuff just changed. We only need to do
  // this initially, because changes after this will automatically fire alert
  // events if things change materially.
  let ev = new CustomEvent("AlertActive", { bubbles: true, cancelable: true });
  document.documentElement.dispatchEvent(ev);
}

function updateIndicatorState() {
  updateWindowAttr("sharingvideo", webrtcUI.showCameraIndicator);
  updateWindowAttr("sharingaudio", webrtcUI.showMicrophoneIndicator);
  updateWindowAttr("sharingscreen", webrtcUI.showScreenSharingIndicator);

  // Camera and microphone button tooltip.
  const audioVideoButton = document.getElementById("audioVideoButton");
  let avL10nId;
  if (webrtcUI.showCameraIndicator) {
    avL10nId = webrtcUI.showMicrophoneIndicator
      ? "webrtc-indicator-sharing-camera-and-microphone"
      : "webrtc-indicator-sharing-camera";
  } else {
    avL10nId = webrtcUI.showMicrophoneIndicator
      ? "webrtc-indicator-sharing-microphone"
      : "";
  }
  if (avL10nId) {
    document.l10n.setAttributes(audioVideoButton, avL10nId);
  } else {
    audioVideoButton.removeAttribute("data-l10n-id");
    audioVideoButton.removeAttribute("tooltiptext");
  }

  // Screen sharing button tooltip.
  const screenShareButton = document.getElementById("screenShareButton");
  let ssL10nId;
  const ssi = webrtcUI.showScreenSharingIndicator;
  switch (ssi) {
    case "Application":
      ssL10nId = "webrtc-indicator-sharing-application";
      break;
    case "Browser":
      ssL10nId = "webrtc-indicator-sharing-browser";
      break;
    case "Screen":
      ssL10nId = "webrtc-indicator-sharing-screen";
      break;
    case "Window":
      ssL10nId = "webrtc-indicator-sharing-window";
      break;
    default:
      if (ssi) {
        console.error(`Unknown showScreenSharingIndicator: ${ssi}`);
      }
      ssL10nId = "";
  }
  if (ssL10nId) {
    document.l10n.setAttributes(screenShareButton, ssL10nId);
  } else {
    screenShareButton.removeAttribute("data-l10n-id");
    screenShareButton.removeAttribute("tooltiptext");
  }

  // Resize and ensure the window position is correct
  // (sizeToContent messes with our position).
  window.sizeToContent();
  PositionHandler.adjustPosition();
}

function updateWindowAttr(attr, value) {
  let docEl = document.documentElement;
  if (value) {
    docEl.setAttribute(attr, "true");
  } else {
    docEl.removeAttribute(attr);
  }
}

function onPopupMenuShowing(event) {
  let popup = event.target;

  let activeStreams;
  if (popup.getAttribute("type") == "Devices") {
    activeStreams = webrtcUI.getActiveStreams(true, true, false);
  } else {
    activeStreams = webrtcUI.getActiveStreams(false, false, true, true);
  }
  if (activeStreams.length) {
    let index = activeStreams.length - 1;
    webrtcUI.showSharingDoorhanger(activeStreams[index], event);
    event.preventDefault();
    return;
  }

  for (let stream of activeStreams) {
    let item = document.createElement("menuitem");
    item.setAttribute("label", stream.browser.contentTitle || stream.uri);
    item.setAttribute("tooltiptext", stream.uri);
    item.stream = stream;
    popup.appendChild(item);
  }
}

function onPopupMenuHiding(event) {
  let popup = event.target;
  while (popup.firstChild) {
    popup.firstChild.remove();
  }
}

function onPopupMenuCommand(event) {
  webrtcUI.showSharingDoorhanger(event.target.stream, event);
}

function onFirefoxButtonClick(event) {
  event.target.blur();
  let activeStreams = webrtcUI.getActiveStreams(true, true, true, true);
  activeStreams[0].browser.ownerGlobal.focus();
}

var PositionHandler = {
  positionCustomized: false,
  threshold: 10,
  adjustPosition() {
    if (!this.positionCustomized) {
      // Center the window horizontally on the screen (not the available area).
      // Until we have moved the window to y=0, 'screen.width' may give a value
      // for a secondary screen, so use values from the screen manager instead.
      let primaryScreen = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
        Ci.nsIScreenManager
      ).primaryScreen;
      let widthDevPix = {};
      primaryScreen.GetRect({}, {}, widthDevPix, {});
      let availTopDevPix = {};
      primaryScreen.GetAvailRect({}, availTopDevPix, {}, {});
      let scaleFactor = primaryScreen.defaultCSSScaleFactor;
      let widthCss = widthDevPix.value / scaleFactor;
      window.moveTo(
        (widthCss - document.documentElement.clientWidth) / 2,
        availTopDevPix.value / scaleFactor
      );
    } else {
      // This will ensure we're at y=0.
      this.setXPosition(window.screenX);
    }
  },
  setXPosition(desiredX) {
    // Ensure the indicator isn't moved outside the available area of the screen.
    desiredX = Math.max(desiredX, screen.availLeft);
    let maxX =
      screen.availLeft +
      screen.availWidth -
      document.documentElement.clientWidth;
    window.moveTo(Math.min(desiredX, maxX), screen.availTop);
  },
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        if (aEvent.button != 0 || aEvent.defaultPrevented) {
          return;
        }

        this._startMouseX = aEvent.screenX;
        this._startWindowX = window.screenX;
        this._deltaX = this._startMouseX - this._startWindowX;

        window.addEventListener("mousemove", this);
        window.addEventListener("mouseup", this);
        break;

      case "mousemove":
        let moveOffset = Math.abs(aEvent.screenX - this._startMouseX);
        if (this._dragFullyStarted || moveOffset > this.threshold) {
          this.setXPosition(aEvent.screenX - this._deltaX);
          this._dragFullyStarted = true;
        }
        break;

      case "mouseup":
        this._dragFullyStarted = false;
        window.removeEventListener("mousemove", this);
        window.removeEventListener("mouseup", this);
        this.positionCustomized =
          Math.abs(this._startWindowX - window.screenX) >= this.threshold;
        break;
    }
  },
};
