/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "CharsetMenu" ];

const { classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "gBundle", function() {
  const kUrl = "chrome://browser/locale/charsetMenu.properties";
  return Services.strings.createBundle(kUrl);
});
/**
 * This set contains encodings that are in the Encoding Standard, except:
 *  - XSS-dangerous encodings (except ISO-2022-JP which is assumed to be
 *    too common not to be included).
 *  - x-user-defined, which practically never makes sense as an end-user-chosen
 *    override.
 *  - Encodings that IE11 doesn't have in its correspoding menu.
 */
const kEncodings = new Set([
  // Globally relevant
  "UTF-8",
  "windows-1252",
  // Arabic
  "windows-1256",
  "ISO-8859-6",
  // Baltic
  "windows-1257",
  "ISO-8859-4",
  // "ISO-8859-13", // Hidden since not in menu in IE11
  // Central European
  "windows-1250",
  "ISO-8859-2",
  // Chinese, Simplified
  "gbk",
  "gb18030",
  // Chinese, Traditional
  "Big5",
  // Cyrillic
  "windows-1251",
  "ISO-8859-5",
  "KOI8-R",
  "KOI8-U",
  "IBM866", // Not in menu in Chromium. Maybe drop this?
  // "x-mac-cyrillic", // Not in menu in IE11 or Chromium.
  // Greek
  "windows-1253",
  "ISO-8859-7",
  // Hebrew
  "windows-1255",
  "ISO-8859-8-I",
  "ISO-8859-8",
  // Japanese
  "Shift_JIS",
  "EUC-JP",
  "ISO-2022-JP",
  // Korean
  "EUC-KR",
  // Thai
  "windows-874",
  // Turkish
  "windows-1254",
  // Vietnamese
  "windows-1258",
  // Hiding rare European encodings that aren't in the menu in IE11 and would
  // make the menu messy by sorting all over the place
  // "ISO-8859-3",
  // "ISO-8859-10",
  // "ISO-8859-14",
  // "ISO-8859-15",
  // "ISO-8859-16",
  // "macintosh"
]);

// Always at the start of the menu, in this order, followed by a separator.
const kPinned = [
  "UTF-8",
  "windows-1252"
];

this.CharsetMenu = Object.freeze({
  build: function BuildCharsetMenu(event, idPrefix="", showAccessKeys=false) {
    let parent = event.target;
    if (parent.lastChild.localName != "menuseparator") {
      // Detector menu or charset menu already built
      return;
    }
    let doc = parent.ownerDocument;

    function createItem(encoding) {
      let menuItem = doc.createElement("menuitem");
      menuItem.setAttribute("type", "radio");
      menuItem.setAttribute("name", "charsetGroup");
      try {
        menuItem.setAttribute("label", gBundle.GetStringFromName(encoding));
      } catch (e) {
        // Localization error but put *something* in the menu to recover.
        menuItem.setAttribute("label", encoding);
      }
      if (showAccessKeys) {
        try {
          menuItem.setAttribute("accesskey",
                                gBundle.GetStringFromName(encoding + ".key"));
        } catch (e) {
          // Some items intentionally don't have an accesskey
        }
      }
      menuItem.setAttribute("id", idPrefix + "charset." + encoding);
      return menuItem;
    }

    // Clone the set in order to be able to remove the pinned encodings from
    // the cloned set.
    let encodings = new Set(kEncodings);
    for (let encoding of kPinned) {
      encodings.delete(encoding);
      parent.appendChild(createItem(encoding));
    }
    parent.appendChild(doc.createElement("menuseparator"));
    let list = [];
    for (let encoding of encodings) {
      list.push(createItem(encoding));
    }

    list.sort(function (a, b) {
      let titleA = a.getAttribute("label");
      let titleB = b.getAttribute("label");
      // Normal sorting sorts the part in parenthesis in an order that
      // happens to make the less frequently-used items first.
      let index;
      if ((index = titleA.indexOf("(")) > -1) {
        titleA = titleA.substring(0, index);
      }
      if ((index = titleB.indexOf("(")) > -1) {
        titleA = titleB.substring(0, index);
      }
      let comp = titleA.localeCompare(titleB);
      if (comp) {
        return comp;
      }
      // secondarily reverse sort by encoding name to sort "windows" or
      // "shift_jis" first. This works regardless of localization, because
      // the ids aren't localized.
      let idA = a.getAttribute("id");
      let idB = b.getAttribute("id");
      if (idA < idB) {
        return 1;
      }
      if (idB < idA) {
        return -1;
      }
      return 0;
    });

    for (let item of list) {
      parent.appendChild(item);
    }
  },
});
