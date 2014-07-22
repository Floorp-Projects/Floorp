/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newWin;
function test() {
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

  requestLongerTimeout(2);
  waitForExplicitFinish();

  // open a new window and setup the window state.
  newWin = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", "about:blank");
  whenWindowLoaded(newWin, function () {
    let newState = {
      windows: [{
        tabs: [{
          entries: [{ "url": "about:blank" }],
          hidden: true,
          attributes: {},
          extData: {
            "tabview-tab":
              '{"bounds":{"left":20,"top":35,"width":280,"height":210},' +
              '"userSize":null,"url":"about:blank","groupID":1,' + 
              '"imageData":null,"title":null}'
          }
        },{
          entries: [{ url: "about:blank" }],
          index: 1,
          hidden: false,
          attributes: {},
          extData: {
            "tabview-tab": 
              '{"bounds":{"left":375,"top":35,"width":280,"height":210},' + 
              '"userSize":null,"url":"about:blank","groupID":2,' + 
              '"imageData":null,"title":null}'
          }
        }],
        selected:2,
        _closedTabs: [],
        extData: {
          "tabview-groups": '{"nextID":3,"activeGroupId":2}',
          "tabview-group": 
            '{"1":{"bounds":{"left":15,"top":10,"width":320,"height":375},' + 
            '"userSize":null,"title":"","id":1},' + 
            '"2":{"bounds":{"left":380,"top":5,"width":320,"height":375},' + 
            '"userSize":null,"title":"","id":2}}',
          "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":875,"height":650}}'
        }, sizemode:"normal"
      }]
    };
    ss.setWindowState(newWin, JSON.stringify(newState), true);

    // add a new tab.
    newWin.gBrowser.addTab();
    is(newWin.gBrowser.tabs.length, 3, "There are 3 browser tabs"); 

    let onTabViewShow = function() {
      newWin.removeEventListener("tabviewshown", onTabViewShow, false);

      let contentWindow = newWin.TabView.getContentWindow();
      is(contentWindow.GroupItems.groupItems.length, 2, "Has two group items");

      // clean up and finish
      newWin.close();

      finish();
    }
    newWin.addEventListener("tabviewshown", onTabViewShow, false);
    waitForFocus(function() { newWin.TabView.toggle(); });
  });
}
