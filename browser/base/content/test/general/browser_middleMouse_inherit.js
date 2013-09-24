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
    let url = "javascript:1+1";
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
      is(gBrowser.currentURI.spec, url, "url loaded by middle click");
      ok(!gBrowser.contentPrincipal.equals(pagePrincipal),
         "middle click load of " + url + " should produce a page with a different principal");
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
