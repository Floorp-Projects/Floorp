/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* A whitelist of images that are loaded at startup but not shown.
 * List items support the following attributes:
 *  - file: The location of the loaded image file.
 *  - hidpi: An alternative hidpi file location for retina screens, if one exists.
 *           May be the magic string <not loaded> in strange cases where
 *           only the low-resolution image is loaded but not shown.
 *  - platforms: An array of the platforms where the issue is occurring.
 *               Possible values are linux, win, macosx.
 *  - intermittentNotLoaded: an array of platforms where this image is
 *                           intermittently not loaded, e.g. because it is
 *                           loaded during the time we stop recording.
 *  - intermittentShown: An array of platforms where this image is
 *                       intermittently shown, contrary to what our
 *                       whitelist says.
 *
 * Please don't add items to this list. Please remove items from this list.
 */
const whitelist = [
  {
    file: "chrome://browser/skin/arrow-left.svg",
    platforms: ["linux", "win", "macosx"],
  },

  {
    file: "chrome://browser/skin/places/toolbarDropMarker.png",
    platforms: ["linux", "win", "macosx"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/tabDragIndicator.png",
    hidpi: "chrome://browser/skin/tabbrowser/tabDragIndicator@2x.png",
    platforms: ["macosx"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/tabDragIndicator.png",
    hidpi: "<not loaded>",
    platforms: ["linux", "win"],
  },

  {
    file: "resource://gre-resources/loading-image.png",
    platforms: ["win", "macosx"],
    intermittentNotLoaded: ["win", "macosx"],
  },
  {
    file: "resource://gre-resources/broken-image.png",
    platforms: ["win", "macosx"],
    intermittentNotLoaded: ["win", "macosx"],
  },

  {
    file: "chrome://browser/skin/chevron.svg",
    platforms: ["win", "linux", "macosx"],
    intermittentShown: ["win", "linux"],
  },

  {
    file: "chrome://global/skin/icons/resizer.svg",
    platforms: ["win"],
  },

  {
    file: "chrome://browser/skin/window-controls/maximize.svg",
    platforms: ["win"],
    // This is to prevent perma-fails in case Windows machines
    // go back to running tests in non-maximized windows.
    intermittentShown: ["win"],
    // This file is not loaded on Windows 7/8.
    intermittentNotLoaded: ["win"],
  },
];

add_task(async function() {
  if (!AppConstants.DEBUG) {
    ok(false, "You need to run this test on a debug build.");
  }

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject;
  await startupRecorder.done;

  let data = Cu.cloneInto(startupRecorder.data.images, {});
  let filteredWhitelist = whitelist.filter(el => {
    return el.platforms.includes(AppConstants.platform);
  });

  let loadedImages = data["image-loading"];
  let shownImages = data["image-drawing"];

  for (let loaded of loadedImages.values()) {
    let whitelistItem = filteredWhitelist.find(el => {
      if (window.devicePixelRatio >= 2 && el.hidpi && el.hidpi == loaded) {
        return true;
      }
      return el.file == loaded;
    });
    if (whitelistItem) {
      if (!whitelistItem.intermittentShown ||
          !whitelistItem.intermittentShown.includes(AppConstants.platform)) {
        todo(shownImages.has(loaded), `Whitelisted image ${loaded} should not have been shown.`);
      }
      continue;
    }
    ok(shownImages.has(loaded), `Loaded image ${loaded} should have been shown.`);
  }

  // Check for unneeded whitelist entries.
  for (let item of filteredWhitelist) {
    if (!item.intermittentNotLoaded ||
        !item.intermittentNotLoaded.includes(AppConstants.platform)) {
      if (window.devicePixelRatio >= 2 && item.hidpi) {
        if (item.hidpi != "<not loaded>") {
          ok(loadedImages.has(item.hidpi), `Whitelisted image ${item.hidpi} should have been loaded.`);
        }
      } else {
        ok(loadedImages.has(item.file), `Whitelisted image ${item.file} should have been loaded.`);
      }
    }
  }
});
