/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// Helper file for shared image functionality
// 
// Note that this is use by tests elsewhere in the source tree. When in doubt,
// check mxr before removing or changing functionality.

// Helper function to clear the image cache of content images
function clearImageCache()
{
  var tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"]
                             .getService(SpecialPowers.Ci.imgITools);
  var imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(false); // true=chrome, false=content
}

// Helper function to determine if the frame is decoded for a given image id
function isFrameDecoded(id)
{
  return (getImageStatus(id) &
          SpecialPowers.Ci.imgIRequest.STATUS_FRAME_COMPLETE)
         ? true : false;
}

// Helper function to determine if the image is loaded for a given image id
function isImageLoaded(id)
{
  return (getImageStatus(id) &
          SpecialPowers.Ci.imgIRequest.STATUS_LOAD_COMPLETE)
         ? true : false;
}

// Helper function to get the status flags of an image
function getImageStatus(id)
{
  // Escalate
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  // Get the image
  var img = SpecialPowers.wrap(document.getElementById(id));

  // QI the image to nsImageLoadingContent
  img.QueryInterface(SpecialPowers.Ci.nsIImageLoadingContent);

  // Get the request
  var request = img.getRequest(SpecialPowers.Ci
                                         .nsIImageLoadingContent
                                         .CURRENT_REQUEST);

  // Return the status
  return request.imageStatus;
}

// Forces a synchronous decode of an image by drawing it to a canvas. Only
// really meaningful if the image is fully loaded first
function forceDecode(id)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  // Get the image
  var img = document.getElementById(id);

  // Make a new canvas
  var canvas = document.createElement("canvas");

  // Draw the image to the canvas. This forces a synchronous decode
  var ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0);
}


// Functions to facilitate getting/setting various image-related prefs
//
// If you change a pref in a mochitest, Don't forget to reset it to its 
// original value!
//
// Null indicates no pref set

const DISCARD_ENABLED_PREF = {name: "discardable", branch: "image.mem.", type: "bool"};
const DECODEONDRAW_ENABLED_PREF = {name: "decodeondraw", branch: "image.mem.", type: "bool"};
const DISCARD_TIMEOUT_PREF = {name: "min_discard_timeout_ms", branch: "image.mem.", type: "int"};

function setImagePref(pref, val)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var branch = prefService.getBranch(pref.branch);
  if (val != null) {
    switch(pref.type) {
      case "bool":
        branch.setBoolPref(pref.name, val);
        break;
      case "int":
        branch.setIntPref(pref.name, val);
        break;
      default:
        throw new Error("Unknown pref type");
    }
  }
  else if (branch.prefHasUserValue(pref.name))
    branch.clearUserPref(pref.name);
}

function getImagePref(pref)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var branch = prefService.getBranch(pref.branch);
  if (branch.prefHasUserValue(pref.name)) {
    switch (pref.type) {
      case "bool":
        return branch.getBoolPref(pref.name);
      case "int":
        return branch.getIntPref(pref.name);
      default:
        throw new Error("Unknown pref type");
    }
  }
  else
    return null;
}

// JS implementation of imgIScriptedNotificationObserver with stubs for all of its methods.
function ImageDecoderObserverStub()
{
  this.sizeAvailable = function sizeAvailable(aRequest)   {}
  this.frameComplete = function frameComplete(aRequest)   {}
  this.decodeComplete = function decodeComplete(aRequest) {}
  this.loadComplete = function loadComplete(aRequest)     {}
  this.frameUpdate = function frameUpdate(aRequest)       {}
  this.discard = function discard(aRequest)               {}
  this.isAnimated = function isAnimated(aRequest)         {}
}
