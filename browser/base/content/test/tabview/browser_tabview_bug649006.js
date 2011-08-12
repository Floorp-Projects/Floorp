/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let contentWindow;
let contentElement;
let groupItem;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    hideTabView();
  });

  showTabView(function() {
    contentWindow = TabView.getContentWindow();
    contentElement = contentWindow.document.getElementById("content");
    test1();
  });
}

function test1() {
  is(gBrowser.tabs.length, 1, 
     "Total number of tabs is 1 before right button double click");
  // first click
  mouseClick(contentElement, 2);
  // second click
  mouseClick(contentElement, 2);

  is(gBrowser.tabs.length, 1, 
     "Total number of tabs is 1 after right button double click");
  test2();
}


function test2() {
  is(gBrowser.tabs.length, 1, 
     "Total number of tabs is 1 before left, right and left mouse clicks");

  // first click
  mouseClick(contentElement, 0);
  // second click
  mouseClick(contentElement, 2);
  // third click
  mouseClick(contentElement, 0);

  is(gBrowser.tabs.length, 1, 
     "Total number of tabs is 1 before left, right and left mouse clicks");
  test3();
}

function test3() {
  ok(contentWindow.GroupItems.groupItems.length, 1, "Only one group item exists");
  groupItem = contentWindow.GroupItems.groupItems[0];

  is(groupItem.getChildren().length, 1, 
     "The number of tab items in the group is 1 before right button double click");

  // first click
  mouseClick(groupItem.container, 2);
  // second click
  mouseClick(groupItem.container, 2);

  is(groupItem.getChildren().length, 1, 
     "The number of tab items in the group is 1 after right button double click");
  test4();
}

function test4() {
  is(groupItem.getChildren().length, 1, 
     "The number of tab items in the group is 1 before left, right, left mouse clicks");

  // first click
  mouseClick(groupItem.container, 0);
  // second click
  mouseClick(groupItem.container, 2);
  // third click
  mouseClick(groupItem.container, 0);

  is(groupItem.getChildren().length, 1, 
     "The number of tab items in the group is 1 before left, right, left mouse clicks");

  hideTabView(function() {
    is(gBrowser.tabs.length, 1, "Total number of tabs is 1 after all tests");
    finish();
  });
}

function mouseClick(targetElement, buttonCode) {
  EventUtils.sendMouseEvent(
    { type: "mousedown", button: buttonCode }, targetElement, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup", button: buttonCode }, targetElement, contentWindow);
}

