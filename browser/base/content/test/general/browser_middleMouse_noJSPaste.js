/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const middleMousePastePref = "middlemouse.contentLoadURL";
const autoScrollPref = "general.autoScroll";
function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(middleMousePastePref, true);
  Services.prefs.setBoolPref(autoScrollPref, false);
  let tab = gBrowser.selectedTab = gBrowser.addTab();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(middleMousePastePref);
    Services.prefs.clearUserPref(autoScrollPref);
    gBrowser.removeTab(tab);
  });

  addPageShowListener(function () {
    let pagePrincipal = gBrowser.contentPrincipal;

    // copy javascript URI to the clipboard
    let url = "javascript:http://www.example.com/";
    waitForClipboard(url,
      function() {
        Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                  .getService(Components.interfaces.nsIClipboardHelper)
                  .copyString(url, document);
      },
      function () {
        // Middle click on the content area
        info("Middle clicking");
        EventUtils.sendMouseEvent({type: "click", button: 1}, gBrowser);
      },
      function() {
        ok(false, "Failed to copy URL to the clipboard");
        finish();
      }
    );

    addPageShowListener(function () {
      is(gBrowser.currentURI.spec, url.replace(/^javascript:/, ""), "url loaded by middle click doesn't include JS");
      finish();
    });
  });
}

function addPageShowListener(func) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    func();
  });
}
