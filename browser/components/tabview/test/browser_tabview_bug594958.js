/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let win;
  let cw;

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let finishTest = function () {
    win.close();
    finish();
  }

  // very long page that produces black bars at the right
  let html1 = '<html><body style="background-color: %2300f;">' +
              '<div style="background: %23fff; width: 95%; height: 10000px; ' +
              ' margin: 0 auto;"></div></body></html>';

  // very short page that produces black bars at the bottom
  let html2 = '<html><body style="background-color: %2300f;"></body></html>';

  let tests = [{
    url: 'data:text/html,' + html1,
    colors: [
      {right: [255, 255, 255], bottom: [255, 255, 255]},
      {right: [255, 255, 255], bottom: [255, 255, 255]},
      {right: [255, 255, 255], bottom: [255, 255, 255]}
    ]
  }, {
    url: 'about:blank',
    colors: [
      {right: [255, 255, 255], bottom: [255, 255, 255]},
      {right: [255, 255, 255], bottom: [255, 255, 255]},
      {right: [255, 255, 255], bottom: [255, 255, 255]}
    ]
  }, {
    url: 'data:text/html,' + html2,
    colors: [
      {right: [0, 0, 255], bottom: [0, 0, 255]},
      {right: [0, 0, 255], bottom: [0, 0, 255]},
      {right: [0, 0, 255], bottom: [0, 0, 255]}
    ]
  }];

  let next = function () {
    if (!tests.length) {
      finishTest();
      return;
    }

    let test = tests.shift();
    let tab = win.gBrowser.tabs[0];

    let browser = tab.linkedBrowser;
    browser.addEventListener("load", function onLoad(event) {
       browser.removeEventListener("load", onLoad, true);
       
       let tabItem = tab._tabViewTabItem;
       tabItem.addSubscriber("updated", function onUpdated() {
         tabItem.removeSubscriber("updated", onUpdated);
         checkUrl(test);
       });
    }, true);
    browser.loadURI(test.url);
  }

  let checkUrl = function (test) {
    let sizes = [[-400, 0], [800, -200], [-400, 200]];

    let nextSize = function () {
      if (!sizes.length) {
        next();
        return;
      }

      let size = sizes.shift();
      let colors = test.colors.shift();
      let groupItem = getGroupItem(0);

      let checkWithSmallItemSize = function () {
        groupItem.setSize(150, 150, true);
        groupItem.setUserSize();

        afterAllTabItemsUpdated(function () {
          checkPixelColors(test.url, colors, nextSize);
        }, win);
      }

      let checkWithNormalItemSize = function () {
        groupItem.setSize(300, 300, true);
        groupItem.setUserSize();

        afterAllTabItemsUpdated(function () {
          checkPixelColors(test.url, colors, checkWithSmallItemSize);
        }, win);
      }

      win.resizeBy.apply(win, size);
      checkWithNormalItemSize();
    }

    nextSize();
  }

  let checkPixelColors = function (url, colors, callback) {
    let tab = win.gBrowser.tabs[0];
    let $canvas = tab._tabViewTabItem.$canvas;
    // Use the direct canvas sizes instead of querying
    // iQ bounds, which may not be current.
    let width = $canvas[0].width;
    let height = $canvas[0].height;

    let ctx = $canvas[0].getContext("2d");

    afterAllTabItemsUpdated(function () {
      checkPixelColor(ctx, url, colors.bottom, Math.floor(width / 4), height - 1, 'bottom');
      checkPixelColor(ctx, url, colors.right, width - 1, Math.floor(height / 4), 'right');
      callback();
    }, win);
  }

  let checkPixelColor = function (ctx, url, color, x, y, edge) {
    let data = ctx.getImageData(x, y, 1, 1).data;
    let check = (data[0] == color[0] && data[1] == color[1] && data[2] == color[2]);
    ok(check, url + ': ' + edge + ' edge color matches pixel value');
  }

  waitForExplicitFinish();
  newWindowWithTabView(function(newWin) {
    win = newWin;

    registerCleanupFunction(function () {
      if (!win.closed)
        win.close();
    });

    cw = win.TabView.getContentWindow();
    next();
  }, null, 800, 600);
}

