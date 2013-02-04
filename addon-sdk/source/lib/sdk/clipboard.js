/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable"
};

const { Cc, Ci } = require("chrome");
const { DataURL } = require("./url");
const errors = require("./deprecated/errors");
const apiUtils = require("./deprecated/api-utils");
/*
While these data flavors resemble Internet media types, they do
no directly map to them.
*/
const kAllowableFlavors = [
  "text/unicode",
  "text/html",
  "image/png"
  /* CURRENTLY UNSUPPORTED FLAVORS
  "text/plain",
  "image/jpg",
  "image/jpeg",
  "image/gif",
  "text/x-moz-text-internal",
  "AOLMAIL",
  "application/x-moz-file",
  "text/x-moz-url",
  "text/x-moz-url-data",
  "text/x-moz-url-desc",
  "text/x-moz-url-priv",
  "application/x-moz-nativeimage",
  "application/x-moz-nativehtml",
  "application/x-moz-file-promise-url",
  "application/x-moz-file-promise-dest-filename",
  "application/x-moz-file-promise",
  "application/x-moz-file-promise-dir"
  */
];

/*
Aliases for common flavors. Not all flavors will
get an alias. New aliases must be approved by a
Jetpack API druid.
*/
const kFlavorMap = [
  { short: "text", long: "text/unicode" },
  { short: "html", long: "text/html" },
  { short: "image", long: "image/png" }
];

let clipboardService = Cc["@mozilla.org/widget/clipboard;1"].
                       getService(Ci.nsIClipboard);

let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].
                      getService(Ci.nsIClipboardHelper);

let imageTools = Cc["@mozilla.org/image/tools;1"].
               getService(Ci.imgITools);

exports.set = function(aData, aDataType) {

  let options = {
    data: aData,
    datatype: aDataType || "text"
  };

  // If `aDataType` is not given or if it's "image", the data is parsed as
  // data URL to detect a better datatype
  if (aData && (!aDataType || aDataType === "image")) {
    try {
      let dataURL = new DataURL(aData);

      options.datatype = dataURL.mimeType;
      options.data = dataURL.data;
    }
    catch (e if e.name === "URIError") {
      // Not a valid Data URL
    }
  }

  options = apiUtils.validateOptions(options, {
    data: {
      is: ["string"]
    },
    datatype: {
      is: ["string"]
    }
  });

  let flavor = fromJetpackFlavor(options.datatype);

  if (!flavor)
    throw new Error("Invalid flavor for " + options.datatype);

  // Additional checks for using the simple case
  if (flavor == "text/unicode") {
    clipboardHelper.copyString(options.data);
    return true;
  }

  // Below are the more complex cases where we actually have to work with a
  // nsITransferable object
  var xferable = Cc["@mozilla.org/widget/transferable;1"].
                 createInstance(Ci.nsITransferable);
  if (!xferable)
    throw new Error("Couldn't set the clipboard due to an internal error " +
                    "(couldn't create a Transferable object).");
  // Bug 769440: Starting with FF16, transferable have to be inited
  if ("init" in xferable)
    xferable.init(null);

  switch (flavor) {
    case "text/html":
      // add text/html flavor
      let (str = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString))
      {
        str.data = options.data;
        xferable.addDataFlavor(flavor);
        xferable.setTransferData(flavor, str, str.data.length * 2);
      }

      // add a text/unicode flavor (html converted to plain text)
      let (str = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString),
           converter = Cc["@mozilla.org/feed-textconstruct;1"].
                       createInstance(Ci.nsIFeedTextConstruct))
      {
        converter.type = "html";
        converter.text = options.data;
        str.data = converter.plainText();
        xferable.addDataFlavor("text/unicode");
        xferable.setTransferData("text/unicode", str, str.data.length * 2);
      }
      break;

    // Set images to the clipboard is not straightforward, to have an idea how
    // it works on platform side, see:
    // http://mxr.mozilla.org/mozilla-central/source/content/base/src/nsCopySupport.cpp?rev=7857c5bff017#530
    case "image/png":
      let image = options.data;

      let container = {};

      try {
        let input = Cc["@mozilla.org/io/string-input-stream;1"].
                      createInstance(Ci.nsIStringInputStream);

        input.setData(image, image.length);

        imageTools.decodeImageData(input, flavor, container);
      }
      catch (e) {
        throw new Error("Unable to decode data given in a valid image.");
      }

      // Store directly the input stream makes the cliboard's data available
      // for Firefox but not to the others application or to the OS. Therefore,
      // a `nsISupportsInterfacePointer` object that reference an `imgIContainer`
      // with the image is needed.
      var imgPtr = Cc["@mozilla.org/supports-interface-pointer;1"].
                     createInstance(Ci.nsISupportsInterfacePointer);

      imgPtr.data = container.value;

      xferable.addDataFlavor(flavor);
      xferable.setTransferData(flavor, imgPtr, -1);

      break;
    default:
      throw new Error("Unable to handle the flavor " + flavor + ".");
  }

  // TODO: Not sure if this will ever actually throw. -zpao
  try {
    clipboardService.setData(
      xferable,
      null,
      clipboardService.kGlobalClipboard
    );
  } catch (e) {
    throw new Error("Couldn't set clipboard data due to an internal error: " + e);
  }
  return true;
};


exports.get = function(aDataType) {
  let options = {
    datatype: aDataType
  };

  // Figure out the best data type for the clipboard's data, if omitted
  if (!aDataType) {
    if (~currentFlavors().indexOf("image"))
      options.datatype = "image";
    else
      options.datatype = "text";
  }

  options = apiUtils.validateOptions(options, {
    datatype: {
      is: ["string"]
    }
  });

  var xferable = Cc["@mozilla.org/widget/transferable;1"].
                 createInstance(Ci.nsITransferable);
  if (!xferable)
    throw new Error("Couldn't set the clipboard due to an internal error " +
                    "(couldn't create a Transferable object).");
  // Bug 769440: Starting with FF16, transferable have to be inited
  if ("init" in xferable)
    xferable.init(null);

  var flavor = fromJetpackFlavor(options.datatype);

  // Ensure that the user hasn't requested a flavor that we don't support.
  if (!flavor)
    throw new Error("Getting the clipboard with the flavor '" + flavor +
                    "' is not supported.");

  // TODO: Check for matching flavor first? Probably not worth it.

  xferable.addDataFlavor(flavor);
  // Get the data into our transferable.
  clipboardService.getData(
    xferable,
    clipboardService.kGlobalClipboard
  );

  var data = {};
  var dataLen = {};
  try {
    xferable.getTransferData(flavor, data, dataLen);
  } catch (e) {
    // Clipboard doesn't contain data in flavor, return null.
    return null;
  }

  // There's no data available, return.
  if (data.value === null)
    return null;

  // TODO: Add flavors here as we support more in kAllowableFlavors.
  switch (flavor) {
    case "text/unicode":
    case "text/html":
      data = data.value.QueryInterface(Ci.nsISupportsString).data;
      break;
    case "image/png":
      let dataURL = new DataURL();

      dataURL.mimeType = flavor;
      dataURL.base64 = true;

      let image = data.value;

      // Due to the differences in how images could be stored in the clipboard
      // the checks below are needed. The clipboard could already provide the
      // image as byte streams, but also as pointer, or as image container.
      // If it's not possible obtain a byte stream, the function returns `null`.
      if (image instanceof Ci.nsISupportsInterfacePointer)
        image = image.data;

      if (image instanceof Ci.imgIContainer)
        image = imageTools.encodeImage(image, flavor);

      if (image instanceof Ci.nsIInputStream) {
        let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                              createInstance(Ci.nsIBinaryInputStream);

        binaryStream.setInputStream(image);

        dataURL.data = binaryStream.readBytes(binaryStream.available());

        data = dataURL.toString();
      }
      else
        data = null;

      break;
    default:
      data = null;
  }

  return data;
};

function currentFlavors() {
  // Loop over kAllowableFlavors, calling hasDataMatchingFlavors for each.
  // This doesn't seem like the most efficient way, but we can't get
  // confirmation for specific flavors any other way. This is supposed to be
  // an inexpensive call, so performance shouldn't be impacted (much).
  var currentFlavors = [];
  for each (var flavor in kAllowableFlavors) {
    var matches = clipboardService.hasDataMatchingFlavors(
      [flavor],
      1,
      clipboardService.kGlobalClipboard
    );
    if (matches)
      currentFlavors.push(toJetpackFlavor(flavor));
  }
  return currentFlavors;
};

Object.defineProperty(exports, "currentFlavors", { get : currentFlavors });

// SUPPORT FUNCTIONS ////////////////////////////////////////////////////////

function toJetpackFlavor(aFlavor) {
  for each (let flavorMap in kFlavorMap)
    if (flavorMap.long == aFlavor)
      return flavorMap.short;
  // Return null in the case where we don't match
  return null;
}

function fromJetpackFlavor(aJetpackFlavor) {
  // TODO: Handle proper flavors better
  for each (let flavorMap in kFlavorMap)
    if (flavorMap.short == aJetpackFlavor || flavorMap.long == aJetpackFlavor)
      return flavorMap.long;
  // Return null in the case where we don't match.
  return null;
}
