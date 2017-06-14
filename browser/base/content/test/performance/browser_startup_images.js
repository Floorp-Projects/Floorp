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
    file: "chrome://browser/skin/fxa/sync-illustration.svg",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/tabbrowser/tab-overflow-indicator.png",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/stop.svg",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/sidebars.svg",
    platforms: ["linux", "win", "macosx"],
    intermittentNotLoaded: ["macosx"],
  },
  {
    file: "chrome://pocket-shared/skin/pocket.svg",
    platforms: ["linux", "win", "macosx"],
    intermittentNotLoaded: ["macosx"],
  },
  {
    file: "chrome://browser/skin/places/toolbarDropMarker.png",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/bookmark-hollow.svg",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/tracking-protection-16.svg#enabled",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/toolbarbutton-dropdown-arrow.png",
    platforms: ["win"],
  },
  {
    file: "chrome://global/skin/icons/autoscroll.png",
    platforms: ["linux", "win", "macosx"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/tab-background-end.png",
    hidpi: "chrome://browser/skin/tabbrowser/tab-background-end@2x.png",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/tabbrowser/tab-background-middle.png",
    hidpi: "chrome://browser/skin/tabbrowser/tab-background-middle@2x.png",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/tabbrowser/tab-background-start.png",
    hidpi: "chrome://browser/skin/tabbrowser/tab-background-start@2x.png",
    platforms: ["linux", "win", "macosx"],
  },
  {
    file: "chrome://browser/skin/tabbrowser/tabDragIndicator.png",
    hidpi: "chrome://browser/skin/tabbrowser/tabDragIndicator@2x.png",
    platforms: ["linux", "win", "macosx"],
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
    file: "chrome://browser/skin/places/unfiledBookmarks.png",
    hidpi: "<not loaded>",
    platforms: ["win", "macosx"],
    intermittentNotLoaded: ["win", "macosx"],
  },
  {
    file: "chrome://browser/skin/urlbar-history-dropmarker.png",
    hidpi: "<not loaded>",
    platforms: ["win", "macosx"],
    intermittentShown: ["win", "macosx"],
  },

  {
    file: "chrome://browser/skin/yosemite/tab-selected-start-inactive.svg",
    platforms: ["macosx"],
  },
  {
    file: "chrome://browser/skin/yosemite/tab-active-middle-inactive.png",
    platforms: ["macosx"],
  },
  {
    file: "chrome://browser/skin/yosemite/tab-selected-end-inactive.svg",
    platforms: ["macosx"],
  },
  {
    file: "chrome://browser/skin/yosemite/tab-stroke-start-inactive.png",
    platforms: ["macosx"],
  },
  {
    file: "chrome://browser/skin/yosemite/tab-stroke-end-inactive.png",
    platforms: ["macosx"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/newtab.png",
    platforms: ["macosx"],
  },

  {
    file: "chrome://global/skin/icons/chevron.png",
    hidpi: "chrome://global/skin/icons/chevron@2x.png",
    platforms: ["macosx"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/alltabs-box-bkgnd-icon.png",
    hidpi: "chrome://browser/skin/tabbrowser/alltabs-box-bkgnd-icon@2x.png",
    platforms: ["macosx"],
  },

  {
    file: "chrome://global/skin/toolbar/chevron.gif",
    platforms: ["win", "linux"],
  },
  {
    file: "chrome://browser/skin/reload-stop-go.png",
    platforms: ["win", "linux"],
    intermittentShown: ["win", "linux"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/alltabs.png",
    platforms: ["linux"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/tab-arrow-left.svg",
    platforms: ["win"],
  },

  {
    file: "chrome://global/skin/icons/resizer.png",
    platforms: ["win"],
  },

  {
    file: "chrome://global/skin/icons/resizer.png",
    platforms: ["win"],
  },

  {
    file: "chrome://browser/skin/tabbrowser/tab-arrow-left.png",
    hidpi: "chrome://browser/skin/tabbrowser/tab-arrow-left@2x.png",
    platforms: ["linux", "macosx"],
  },
  {
    file: "chrome://browser/skin/tabbrowser/tab-arrow-right.png",
    hidpi: "chrome://browser/skin/tabbrowser/tab-arrow-right@2x.png",
    platforms: ["macosx"],
  },
];

function test() {
  let data = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject.data.images;
  let platformWhitelist = whitelist.filter(el => el.platforms.includes(AppConstants.platform));

  let loadedImages = data["image-loading"];
  let shownImages = data["image-drawing"];

  for (let loaded of loadedImages.values()) {
    let whitelistItem = platformWhitelist.find(el => {
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
  for (let item of platformWhitelist) {
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
}
