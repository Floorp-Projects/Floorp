/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let compareCanvasSize = function (prefix, width, height, tabItem) {
    let canvas = tabItem.$canvas[0];
    is(canvas.width, width, prefix + ": canvas widths are equal (" + width + "px)");
    is(canvas.height, height, prefix + ": canvas heights are equal (" + height + "px)");
  };

  newWindowWithTabView(function (win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];
    let tabItem = groupItem.getChild(0);

    afterAllTabItemsUpdated(function () {
      let {width, height} = tabItem.$canvas[0];

      hideGroupItem(groupItem, function () {
        compareCanvasSize("hidden", width, height, tabItem);

        unhideGroupItem(groupItem, function () {
          compareCanvasSize("unhidden", width, height, tabItem);
          finish();
        });
      });
    }, win);
  });
}
