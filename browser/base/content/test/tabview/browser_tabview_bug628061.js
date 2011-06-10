/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

let state = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:blank" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:blank","groupID":1,"bounds":{"left":20,"top":20,"width":20,"height":20}}'}
    },{
      entries: [{ url: "about:blank" }],
      hidden: false,
      extData: {"tabview-tab": '{"url":"about:blank","groupID":2,"bounds":{"left":20,"top":20,"width":20,"height":20}}'},
    }],
    selected: 2,
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":2, "totalNumber":2}',
      "tabview-group":
        '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},"id":1},' +
        '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},"id":2}}'
    }
  }]
};

function test() {
  waitForExplicitFinish();
  testOne();
}

function testOne() {
  newWindowWithTabView(
    function(win) {
      testTwo();
      win.close();
    },
    function(win) {
      registerCleanupFunction(function() win.close());
      is(win.document.getElementById("tabviewGroupsNumber").getAttribute("groups"),
         "1", "There is one group");
    });
}

function testTwo() {
  newWindowWithState(state, function(win) {
    registerCleanupFunction(function() win.close());

    is(win.document.getElementById("tabviewGroupsNumber").getAttribute("groups"),
       "2", "There are two groups");
    waitForFocus(finish);
  });
}
