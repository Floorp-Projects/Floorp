import { isEmbeddedFrame } from "/core/utils/commons.mjs";


/**
 * PopupCommandView "singleton"
 * provides simple function to style the popup
 **/


// public methods and variables


export default {
  get theme () {
    return PopupURL.searchParams.get("theme");
  },
  set theme (value) {
    PopupURL.searchParams.set("theme", value);
  }
};


// private variables and methods

// use HTML namespace so proper HTML elements will be created even in foreign doctypes/namespaces (issue #565)
const Popup = document.createElementNS("http://www.w3.org/1999/xhtml", "iframe");

const PopupURL = new URL(browser.runtime.getURL("/core/views/popup-command-view/popup-command-view.html"));

let mousePositionX = 0,
    mousePositionY = 0;

// setup background/command message event listener for top frame
if (!isEmbeddedFrame()) browser.runtime.onMessage.addListener(handleMessage);


/**
 * Handles background messages
 * Procedure:
 * 1. wait for message to create popup hiddenly
 * 2. wait for message from popup with list dimension and update popup size + show popup / remove opacity
 * 3. wait for popup close message
 **/
function handleMessage (message) {
  switch (message.subject) {
    case "popupRequest": return loadPopup(message.data);

    case "popupInitiation": return initiatePopup(message.data);

    case "popupTermination": return terminatePopup();
  }
}


/**
 * Creates and appends the Popup iframe hiddenly
 * Promise resolves to true if the Popup was created and loaded successfully else false
 **/
async function loadPopup (data) {
  // if popup is still appended to DOM
  if (Popup.isConnected) {
    // trigger the terminate event in the iframe/popup via blur
    // wait for the popup termination message and its removal by the terminatePopup function
    // otherwise the termination message / terminatePopup function may remove the newly created popup
    await new Promise((resolve) => {
      browser.runtime.onMessage.addListener((message) => {
        if (message.subject === "popupTermination") {
          resolve();
        }
      });
      Popup.blur();
    });
  }

  // popup is not working in a pure svg or other xml pages thus cancel the popup creation
  if (!document.body && document.documentElement.namespaceURI !== "http://www.w3.org/1999/xhtml") {
    return false;
  }

  Popup.style = `
      all: initial !important;
      position: fixed !important;
      top: 0 !important;
      left: 0 !important;
      border: 0 !important;
      z-index: 2147483647 !important;
      box-shadow: 1px 1px 4px rgba(0,0,0,0.3) !important;
      opacity: 0 !important;
      transition: opacity .3s !important;
      visibility: hidden !important;
    `;
  const popupLoaded = new Promise((resolve, reject) => {
    Popup.onload = resolve;
  });

  // calc and store correct mouse position
  mousePositionX = data.mousePositionX - window.mozInnerScreenX;
  mousePositionY = data.mousePositionY - window.mozInnerScreenY;

  // appending the element to the DOM will start loading the iframe content

  // if an element is in fullscreen mode and this element is not the document root (html element)
  // append the popup to this element (issue #148)
  if (document.fullscreenElement && document.fullscreenElement !== document.documentElement) {
    document.fullscreenElement.appendChild(Popup);
  }
  else if (document.body.tagName.toUpperCase() === "FRAMESET") {
    document.documentElement.appendChild(Popup);
  }
  else {
    document.body.appendChild(Popup);
  }

  // navigate iframe (from about:blank to extension page) via window.location instead of setting the src
  // this prevents UUID leakage and therefore reduces potential fingerprinting
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1372288#c25
  Popup.contentWindow.location = PopupURL.href;

  // return true when popup is loaded
  await popupLoaded;

  return true;
}


/**
 * Sizes, positions and fades in the popup
 * Returns a promise with the calculated available width and height
 **/
async function initiatePopup (data) {
  const relativeScreenWidth = document.documentElement.clientWidth || document.body.clientWidth || window.innerWidth;
  const relativeScreenHeight = document.documentElement.clientHeight || document.body.clientHeight || window.innerHeight;

  // get the absolute maximum available height from the current position either from the top or bottom
  const maxAvailableHeight = Math.max(relativeScreenHeight - mousePositionY, mousePositionY);

  // get absolute list dimensions
  const width = data.width;
  const height = Math.min(data.height, maxAvailableHeight);

  // calculate absolute available space to the right and bottom
  const availableSpaceRight = (relativeScreenWidth - mousePositionX);
  const availableSpaceBottom = (relativeScreenHeight - mousePositionY);

  // get the ideal relative position based on the given available space and dimensions
  const x = availableSpaceRight >= width ? mousePositionX : mousePositionX - width;
  const y = availableSpaceBottom >= height ? mousePositionY : mousePositionY - height;

  // apply scale, position, dimensions to Popup / iframe and make it visible
  Popup.style.setProperty('width', Math.round(width) + 'px', 'important');
  Popup.style.setProperty('height', Math.round(height) + 'px', 'important');
  Popup.style.setProperty('transform-origin', `0 0`, 'important');
  Popup.style.setProperty('transform', `translate(${Math.round(x)}px, ${Math.round(y)}px)`, 'important');
  Popup.style.setProperty('opacity', '1', 'important');
  Popup.style.setProperty('visibility','visible', 'important');

  return {
    width: width,
    height: height
  }
}


/**
 * Removes the popup from the DOM and resets all variables
 * Returns an empty promise
 **/
async function terminatePopup () {
  Popup.remove();
  // reset variables
  mousePositionX = 0;
  mousePositionY = 0;
}