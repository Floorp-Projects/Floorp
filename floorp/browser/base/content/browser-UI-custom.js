/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//import utils
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const UICustomPrefHandler = function (pref, callback) {
  let prefValue = Services.prefs.getBoolPref(pref, false);
  try {
    callback({
      pref,
      prefValue,
      reason: "init",
    });
  } catch (e) {
    console.error(e);
  }
  Services.prefs.addObserver(pref, function () {
    let prefValue = Services.prefs.getBoolPref(pref, false);
    try {
      callback({
        pref,
        prefValue,
        reason: "changed",
      });
    } catch (e) {
      console.error(e);
    }
  });
};

// prefs
UICustomPrefHandler("floorp.material.effect.enable", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/micaforeveryone.css)`;
    Tag.setAttribute("id", "floorp-micaforeveryone");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-micaforeveryone")?.remove();
  }
});

UICustomPrefHandler(
  "floorp.Tree-type.verticaltab.optimization",
  function (event) {
    if (event.prefValue) {
      let Tag = document.createElement("style");
      Tag.innerText = `@import url(chrome://browser/skin/options/treestyletab.css)`;
      Tag.setAttribute("id", "floorp-optimizefortreestyletab");
      document.head.appendChild(Tag);
    } else {
      document.getElementById("floorp-optimizefortreestyletab")?.remove();
    }
  }
);

UICustomPrefHandler("floorp.optimized.msbutton.ope", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/msbutton.css)`;
    Tag.setAttribute("id", "floorp-optimizedmsbuttonope");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-optimizedmsbuttonope")?.remove();
  }
});

UICustomPrefHandler("floorp.bookmarks.bar.focus.mode", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/bookmarkbar_autohide.css)`;
    Tag.setAttribute("id", "floorp-bookmarkbarfocus");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-bookmarkbarfocus")?.remove();
  }
});

UICustomPrefHandler("floorp.bookmarks.fakestatus.mode", function (event) {
  if (event.prefValue) {
    setTimeout(
      function () {
        document
          .getElementById("fullscreen-and-pointerlock-wrapper")
          .after(document.getElementById("PersonalToolbar"));
      },
      event.reason === "init" ? 250 : 1
    );
  } else if (event.reason === "changed") {
    //Fix for the bug that bookmarksbar is on the navigation toolbar when the pref is cahaned to false
    if (!Services.prefs.getBoolPref("floorp.navbar.bottom", false)) {
      document
        .getElementById("navigator-toolbox")
        .appendChild(document.getElementById("nav-bar"));
    }
    document
      .getElementById("navigator-toolbox")
      .appendChild(document.getElementById("PersonalToolbar"));
  }
});

UICustomPrefHandler("floorp.search.top.mode", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/move_page_inside_searchbar.css)`;
    Tag.setAttribute("id", "floorp-searchbartop");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-searchbartop")?.remove();
  }
});

UICustomPrefHandler("floorp.legacy.dlui.enable", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/browser-custum-dlmgr.css)`;
    Tag.setAttribute("id", "floorp-dlmgrcss");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-dlmgrcss")?.remove();
  }
});

UICustomPrefHandler("floorp.downloading.red.color", function (event) {
  if (event.prefValue) {
    let Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/downloading-redcolor.css`;
    Tag.setAttribute("id", "floorp-dlredcolor");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-dlredcolor")?.remove();
  }
});

UICustomPrefHandler("floorp.navbar.bottom", function (event) {
  if (event.prefValue) {
    var Tag = document.createElement("style");
    Tag.setAttribute("id", "floorp-navvarcss");
    Tag.innerText = `@import url(chrome://browser/skin/options/navbar-botttom.css)`;
    document.head.appendChild(Tag);
    document
      .getElementById("fullscreen-and-pointerlock-wrapper")
      .after(document.getElementById("nav-bar"));
  } else {
    document.getElementById("floorp-navvarcss")?.remove();
    if (event.reason === "changed") {
      //Fix for the bug that bookmarksbar is on the navigation toolbar when the pref is cahaned to false

      document
        .getElementById("navigator-toolbox")
        .appendChild(document.getElementById("nav-bar"));

      if (
        !Services.prefs.getBoolPref("floorp.bookmarks.fakestatus.mode", false)
      ) {
        document
          .getElementById("navigator-toolbox")
          .appendChild(document.getElementById("PersonalToolbar"));
      }
    }
  }
});

UICustomPrefHandler("floorp.disable.fullscreen.notification", function (event) {
  if (event.prefValue) {
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/disableFullScreenNotification.css)`;
    Tag.setAttribute("id", "floorp-DFSN");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-DFSN")?.remove();
  }
});

UICustomPrefHandler("floorp.delete.browser.border", function (event) {
  if (event.prefValue) {
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/delete-border.css)`;
    Tag.setAttribute("id", "floorp-DB");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-DB")?.remove();
  }
});

/*------------------------------------------- sidebar -------------------------------------------*/

if (!Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
  var Tag = document.createElement("style");
  Tag.textContent = `
  #sidebar-button2,
  #wrapper-sidebar-button2,
  .browser-sidebar2,
  #sidebar-select-box {
    display: none !important;
  }`;
  document.head.appendChild(Tag);
}

/*------------------------------------------- verticaltab -------------------------------------------*/

UICustomPrefHandler("floorp.verticaltab.hover.enabled", function (event) {
  if (Services.prefs.getIntPref("floorp.tabbar.style", false) != 2) {
    return;
  }
  if (event.prefValue) {
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/options/native-verticaltab-hover.css)`;
    Tag.setAttribute("id", "floorp-vthover");
    document.head.appendChild(Tag);
  } else {
    document.getElementById("floorp-vthover")?.remove();
  }
});
