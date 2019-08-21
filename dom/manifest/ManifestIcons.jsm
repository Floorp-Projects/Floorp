"use strict";

const { PromiseMessage } = ChromeUtils.import(
  "resource://gre/modules/PromiseMessage.jsm"
);

var ManifestIcons = {
  async browserFetchIcon(aBrowser, manifest, iconSize) {
    const msgKey = "DOM:WebManifest:fetchIcon";
    const mm = aBrowser.messageManager;
    const {
      data: { success, result },
    } = await PromiseMessage.send(mm, msgKey, { manifest, iconSize });
    if (!success) {
      throw result;
    }
    return result;
  },

  async contentFetchIcon(aWindow, manifest, iconSize) {
    return getIcon(aWindow, toIconArray(manifest.icons), iconSize);
  },
};

function parseIconSize(size) {
  if (size === "any" || size === "") {
    // We want icons without size specified to sorted
    // as the largest available icons
    return Number.MAX_SAFE_INTEGER;
  }
  // 100x100 will parse as 100
  return parseInt(size, 10);
}

// Create an array of icons sorted by their size
function toIconArray(icons) {
  const iconBySize = [];
  icons.forEach(icon => {
    const sizes = "sizes" in icon ? icon.sizes : "";
    sizes.forEach(size => {
      iconBySize.push({ src: icon.src, size: parseIconSize(size) });
    });
  });
  return iconBySize.sort((a, b) => a.size - b.size);
}

async function getIcon(aWindow, icons, expectedSize) {
  if (!icons.length) {
    throw new Error("Could not find valid icon");
  }
  // We start trying the smallest icon that is larger than the requested
  // size and go up to the largest icon if they fail, if all those fail
  // go back down to the smallest
  let index = icons.findIndex(icon => icon.size >= expectedSize);
  if (index === -1) {
    index = icons.length - 1;
  }

  return fetchIcon(aWindow, icons[index].src).catch(err => {
    // Remove all icons with the failed source, the same source
    // may have been used for multiple sizes
    icons = icons.filter(x => x.src !== icons[index].src);
    return getIcon(aWindow, icons, expectedSize);
  });
}

async function fetchIcon(aWindow, src) {
  const iconURL = new aWindow.URL(src, aWindow.location);
  const request = new aWindow.Request(iconURL, { mode: "cors" });
  request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_IMAGE);
  return aWindow
    .fetch(request)
    .then(response => response.blob())
    .then(
      blob =>
        new Promise((resolve, reject) => {
          var reader = new FileReader();
          reader.onloadend = () => resolve(reader.result);
          reader.onerror = reject;
          reader.readAsDataURL(blob);
        })
    );
}

var EXPORTED_SYMBOLS = ["ManifestIcons"]; // jshint ignore:line
