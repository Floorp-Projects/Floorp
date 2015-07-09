/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that pinned suggested tile turns into history tile
 * and remains a history tile after a user clicks on it
 */

gDirectorySource = "data:application/json," + JSON.stringify({
  "suggested": [{
    url: "http://example.com/hardlanding/page.html",
    imageURI: "data:image/png;base64,helloWORLD3",
    enhancedImageURI: "data:image/png;base64,helloWORLD2",
    title: "title",
    type: "affiliate",
    frecent_sites: ["example0.com"],
  }]
});

function runTests() {
  let origGetFrecentSitesName = DirectoryLinksProvider.getFrecentSitesName;
  DirectoryLinksProvider.getFrecentSitesName = () => "";

  function getData(cellNum) {
    let cell = getCell(cellNum);
    if (!cell.site)
      return null;
    let siteNode = cell.site.node;
    return {
      type: siteNode.getAttribute("type"),
      thumbnail: siteNode.querySelector(".newtab-thumbnail").style.backgroundImage,
      enhanced: siteNode.querySelector(".enhanced-content").style.backgroundImage,
      title: siteNode.querySelector(".newtab-title").textContent,
      suggested: siteNode.getAttribute("suggested"),
      url: siteNode.querySelector(".newtab-link").getAttribute("href"),
    };
  }

  yield setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks("");

  yield addNewTabPageTab();
  // load another newtab since the first may not get suggested tile
  yield addNewTabPageTab();
  checkGrid("http://example.com/hardlanding/page.html,0,1,2,3,4,5,6,7,8,9");
  // evaluate suggested tile
  let tileData = getData(0);
  is(tileData.type, "affiliate", "unpinned type");
  is(tileData.thumbnail, "url(\"data:image/png;base64,helloWORLD3\")", "unpinned thumbnail");
  is(tileData.enhanced, "url(\"data:image/png;base64,helloWORLD2\")", "unpinned enhanced");
  is(tileData.suggested, "true", "has suggested set", "unpinned suggested exists");
  is(tileData.url, "http://example.com/hardlanding/page.html", "unpinned landing page");

  // suggested tile should not be pinned
  is(NewTabUtils.pinnedLinks.isPinned({url: "http://example.com/hardlanding/page.html"}), false, "suggested tile is not pinned");

  // pin suggested tile
  whenPagesUpdated();
  let siteNode = getCell(0).node.querySelector(".newtab-site");
  let pinButton = siteNode.querySelector(".newtab-control-pin");
  EventUtils.synthesizeMouseAtCenter(pinButton, {}, getContentWindow());
  // wait for whenPagesUpdated
  yield null;

  // tile should be pinned and turned into history tile
  is(NewTabUtils.pinnedLinks.isPinned({url: "http://example.com/hardlanding/page.html"}), true, "suggested tile is pinned");
  tileData = getData(0);
  is(tileData.type, "history", "pinned type");
  is(tileData.suggested, null, "no suggested attribute");
  is(tileData.url, "http://example.com/hardlanding/page.html", "original landing page");

  // click the pinned tile
  siteNode = getCell(0).node.querySelector(".newtab-site");
  EventUtils.synthesizeMouseAtCenter(siteNode, {}, getContentWindow());
  // add new page twice to avoid using cached version
  yield addNewTabPageTab();
  yield addNewTabPageTab();

  // check that type and suggested did not change
  tileData = getData(0);
  is(tileData.type, "history", "pinned type");
  is(tileData.suggested, null, "no suggested attribute");

  DirectoryLinksProvider.getFrecentSitesName = origGetFrecentSitesName;
}
