/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is tabview private browsing test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Ian Gilman <ian@iangilman.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let normalURLs = []; 
let pbTabURL = "about:privatebrowsing";

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

// -----------
function test() {
  waitForExplicitFinish();

  // Establish initial state
  is(gBrowser.tabs.length, 1, "we start with 1 tab");
  
  // Create a second tab
  gBrowser.addTab("about:robots");
  is(gBrowser.tabs.length, 2, "we now have 2 tabs");
  
  afterAllTabsLoaded(function() {
    // Get normal tab urls
    for (let a = 0; a < gBrowser.tabs.length; a++) {
      normalURLs.push(gBrowser.tabs[a].linkedBrowser.currentURI.spec);
    }
  
    // Go into Tab View
    window.addEventListener("tabviewshown", onTabViewLoadedAndShown, false);
    TabView.toggle();
  });
}

// -----------
function onTabViewLoadedAndShown() {
  window.removeEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  // go into private browsing and make sure Tab View becomes hidden
  pb.privateBrowsingEnabled = true;
  ok(!TabView.isVisible(), "Tab View is no longer visible");
  afterAllTabsLoaded(function() {
    verifyPB();
    
    // exit private browsing and make sure Tab View is shown again
    pb.privateBrowsingEnabled = false;
    ok(TabView.isVisible(), "Tab View is visible again");
    afterAllTabsLoaded(function() {
      verifyNormal();
      
      // exit Tab View
      window.addEventListener("tabviewhidden", onTabViewHidden, false);
      TabView.toggle();
    });  
  });
}

// -----------
function onTabViewHidden() {
  window.removeEventListener("tabviewhidden", onTabViewHidden, false);
  ok(!TabView.isVisible(), "Tab View is not visible");
  
  // go into private browsing and make sure Tab View remains hidden
  pb.privateBrowsingEnabled = true;
  ok(!TabView.isVisible(), "Tab View is still not visible");
  afterAllTabsLoaded(function() {
    verifyPB();
    
    // turn private browsing back off
    pb.privateBrowsingEnabled = false;
    afterAllTabsLoaded(function() {
      verifyNormal();
      
      // clean up
      gBrowser.removeTab(gBrowser.tabs[1]);
      
      is(gBrowser.tabs.length, 1, "we finish with one tab");
      ok(!pb.privateBrowsingEnabled, "we finish with private browsing off");
      ok(!TabView.isVisible(), "we finish with Tab View not visible");
    
      finish();
    });
  });
}

// ----------
function verifyPB() {
  ok(pb.privateBrowsingEnabled == true, "private browsing is on");
  is(gBrowser.tabs.length, 1, "we have 1 tab in private browsing");

  let browser = gBrowser.tabs[0].linkedBrowser;
  is(browser.currentURI.spec, pbTabURL, "correct URL for private browsing");
}

// ----------
function verifyNormal() {
  ok(pb.privateBrowsingEnabled == false, "private browsing is off");

  let count = gBrowser.tabs.length;
  is(count, 2, "we have 2 tabs in normal mode");

  for (let a = 0; a < count; a++) {
    let browser = gBrowser.tabs[a].linkedBrowser;
    is(browser.currentURI.spec, normalURLs[a], "correct URL for normal mode");
  }
}

// ----------
function afterAllTabsLoaded(callback) {
  let stillToLoad = 0; 
  function onLoad() {
    this.removeEventListener("load", onLoad, true);
    
    stillToLoad--;
    if (!stillToLoad)
      callback();
  }

  for (let a = 0; a < gBrowser.tabs.length; a++) {
    let browser = gBrowser.tabs[a].linkedBrowser;
    if (browser.webProgress.isLoadingDocument) {
      stillToLoad++;
      browser.addEventListener("load", onLoad, true);
    }
  }
}
