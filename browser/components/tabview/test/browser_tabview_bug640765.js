/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;
let groupItem;

function test() {
  waitForExplicitFinish();

  let newTabOne = gBrowser.addTab();
  let newTabTwo = gBrowser.addTab();
  let newTabThree = gBrowser.addTab();

  registerCleanupFunction(function() {
    TabView.hide();
    while (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.tabs[0]);
  });

  showTabView(function() {
    contentWindow = document.getElementById("tab-view").contentWindow;
    is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

    groupItem = contentWindow.GroupItems.groupItems[0];

    whenAppTabIconAdded(function() {
      whenAppTabIconAdded(function() {
        whenAppTabIconAdded(function() {

          is(xulTabForAppTabIcon(0), newTabOne,
            "New tab one matches the first app tab icon in tabview");
          is(xulTabForAppTabIcon(1), newTabTwo,
            "New tab two matches the second app tab icon in tabview");
          is(xulTabForAppTabIcon(2), newTabThree,
            "New tab three matches the third app tab icon in tabview");

          // move the last tab to the first position
          gBrowser.moveTabTo(newTabThree, 0);
          is(xulTabForAppTabIcon(0), newTabThree,
            "New tab three matches the first app tab icon in tabview");
          is(xulTabForAppTabIcon(1), newTabOne,
            "New tab one matches the second app tab icon in tabview");
          is(xulTabForAppTabIcon(2), newTabTwo,
            "New tab two matches the third app tab icon in tabview");

          // move the first tab to the second position
          gBrowser.moveTabTo(newTabThree, 1);
          is(xulTabForAppTabIcon(0), newTabOne,
            "New tab one matches the first app tab icon in tabview");
          is(xulTabForAppTabIcon(1), newTabThree,
            "New tab three matches the second app tab icon in tabview");
          is(xulTabForAppTabIcon(2), newTabTwo,
            "New tab two matches the third app tab icon in tabview");

          hideTabView(function() {
            gBrowser.removeTab(newTabOne);
            gBrowser.removeTab(newTabTwo);
            gBrowser.removeTab(newTabThree);
            finish();
          });
        });
        gBrowser.pinTab(newTabThree);
      });
      gBrowser.pinTab(newTabTwo);
    });
    gBrowser.pinTab(newTabOne);
  });
}

function xulTabForAppTabIcon(index) {
    return contentWindow.iQ(
             contentWindow.iQ(".appTabIcon", 
                              groupItem.$appTabTray)[index]).data("xulTab");
}

