/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "enhanced": [{
    url: "http://example.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD",
    title: "title",
    type: "organic",
  }],
  "directory": [{
    url: "http://example1.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD2",
    title: "title1",
    type: "organic"
  }],
  "suggested": [{
    url: "http://example1.com/2",
    imageURI: "data:image/png;base64,helloWORLD3",
    title: "title2",
    type: "affiliate",
    frecent_sites: ["test.com"]
  }]
});

function runTests() {
  let origGetFrecentSitesName = DirectoryLinksProvider.getFrecentSitesName;
  DirectoryLinksProvider.getFrecentSitesName = () => "";
  let origEnhanced = NewTabUtils.allPages.enhanced;
  registerCleanupFunction(() => {
    DirectoryLinksProvider.getFrecentSitesName = origGetFrecentSitesName;
    Services.prefs.clearUserPref(PRELOAD_PREF);
    NewTabUtils.allPages.enhanced = origEnhanced;
  });

  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  function getData(cellNum) {
    let cell = getCell(cellNum);
    if (!cell.site)
      return null;
    let siteNode = cell.site.node;
    return {
      type: siteNode.getAttribute("type"),
      enhanced: siteNode.querySelector(".enhanced-content").style.backgroundImage,
      title: siteNode.querySelector(".newtab-title").textContent,
    };
  }

  // Make the page have a directory link, enhanced link, and history link
  yield setLinks("-1");

  // Test with enhanced = false
  yield addNewTabPageTab();
  yield customizeNewTabPage("classic");
  yield customizeNewTabPage("enhanced"); // Toggle enhanced off
  let {type, enhanced, title} = getData(0);
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");

  is(getData(1), null, "there is only one link and it's a history link");

  // Test with enhanced = true
  yield addNewTabPageTab();
  yield customizeNewTabPage("enhanced"); // Toggle enhanced on
  ({type, enhanced, title} = getData(0));
  is(type, "organic", "directory link is organic");
  isnot(enhanced, "", "directory link has enhanced image");
  is(title, "title1");

  ({type, enhanced, title} = getData(1));
  is(type, "enhanced", "history link is enhanced");
  isnot(enhanced, "", "history link has enhanced image");
  is(title, "title");

  is(getData(2), null, "there are only 2 links, directory and enhanced history");

  // Test with a pinned link
  setPinnedLinks("-1");
  yield addNewTabPageTab();
  ({type, enhanced, title} = getData(0));
  is(type, "enhanced", "pinned history link is enhanced");
  isnot(enhanced, "", "pinned history link has enhanced image");
  is(title, "title");

  ({type, enhanced, title} = getData(1));
  is(type, "organic", "directory link is organic");
  isnot(enhanced, "", "directory link has enhanced image");
  is(title, "title1");

  is(getData(2), null, "directory link pushed out by pinned history link");

  // Test pinned link with enhanced = false
  yield addNewTabPageTab();
  yield customizeNewTabPage("enhanced"); // Toggle enhanced off
  ({type, enhanced, title} = getData(0));
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");

  is(getData(1), null, "directory link still pushed out by pinned history link");

  ok(getContentDocument().getElementById("newtab-intro-what"),
     "'What is this page?' link exists");

  yield unpinCell(0);



  // Test that a suggested tile is not enhanced by a directory tile
  let origIsTopPlacesSite = NewTabUtils.isTopPlacesSite;
  NewTabUtils.isTopPlacesSite = () => true;
  yield setLinks("-1,2,3,4,5,6,7,8");

  // Test with enhanced = false
  yield addNewTabPageTab();
  ({type, enhanced, title} = getData(0));
  isnot(type, "enhanced", "history link is not enhanced");
  is(enhanced, "", "history link has no enhanced image");
  is(title, "example.com");

  isnot(getData(7), null, "there are 8 history links");
  is(getData(8), null, "there are 8 history links");


  // Test with enhanced = true
  yield addNewTabPageTab();
  yield customizeNewTabPage("enhanced");

  // Suggested link was not enhanced by directory link with same domain
  ({type, enhanced, title} = getData(0));
  is(type, "affiliate", "suggested link is affiliate");
  is(enhanced, "", "suggested link has no enhanced image");
  is(title, "title2");

  // Enhanced history link shows up second
  ({type, enhanced, title} = getData(1));
  is(type, "enhanced", "pinned history link is enhanced");
  isnot(enhanced, "", "pinned history link has enhanced image");
  is(title, "title");

  is(getData(9), null, "there is a suggested link followed by an enhanced history link and the remaining history links");
}
