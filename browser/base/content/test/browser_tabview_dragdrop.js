function test() {
  waitForExplicitFinish();

  TabView.toggle();
  ok(TabView.isVisible(), "Tab View is visible");  
  
  let contentWindow = document.getElementById("tab-view").contentWindow;

  // create group one and two
  let padding = 10;
  let pageBounds = contentWindow.Items.getPageBounds();
  pageBounds.inset(padding, padding);
  
  let box = new contentWindow.Rect(pageBounds);
  box.width = 300;
  box.height = 300;

  let groupOne = new contentWindow.Group([], { bounds: box });
  ok(groupOne.isEmpty(), "This group is empty");

  let groupTwo = new contentWindow.Group([], { bounds: box });

  groupOne.addSubscriber(groupOne, "tabAdded", function() {
    groupOne.removeSubscriber(groupOne, "tabAdded");
    groupTwo.newTab("");
  });
  groupTwo.addSubscriber(groupTwo, "tabAdded", function() {
    groupTwo.removeSubscriber(groupTwo, "tabAdded");
    // carry on testing
    addTest(contentWindow, groupOne.id, groupTwo.id);
  });

  let count = 0;
  let onTabViewHidden = function() {
    // show the tab view.
    TabView.toggle();
    if (++count == 2) {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    }
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // open tab in group
  groupOne.newTab("");
}


function addTest(contentWindow, groupOneId, groupTwoId) {
  let groupOne = contentWindow.Groups.group(groupOneId);
  let groupTwo = contentWindow.Groups.group(groupTwoId);
  let groupOneTabItemCount = groupOne.getChildren().length;
  let groupTwoTabItemCount = groupTwo.getChildren().length;
  is(groupOneTabItemCount, 1, "Group one has a tab"); 
  is(groupTwoTabItemCount, 1, "Group two has two tabs"); 
  
  let srcElement = groupOne.getChild(0).container;
  ok(srcElement, "The source element exists");
  
  // calculate the offsets
  let groupTwoRect = groupTwo.container.getBoundingClientRect();
  let srcElementRect = srcElement.getBoundingClientRect();
  let offsetX = 
    Math.round(groupTwoRect.left + groupTwoRect.width/5) - srcElementRect.left;
  let offsetY = 
    Math.round(groupTwoRect.top + groupTwoRect.height/5) -  srcElementRect.top;

  simulateDragDrop(srcElement, offsetX, offsetY, contentWindow);
  
  is(groupOne.getChildren().length, --groupOneTabItemCount, 
     "The number of children in group one is decreased by 1");
  is(groupTwo.getChildren().length, ++groupTwoTabItemCount, 
     "The number of children in group two is increased by 1");

  TabView.toggle();
  groupTwo.closeAll(); 

  finish();
}

function simulateDragDrop(srcElement, offsetX, offsetY, contentWindow) {
  // enter drag mode
  let dataTransfer;

  EventUtils.synthesizeMouse(
    srcElement, 1, 1, { type: "mousedown" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0, 
    false, false, false, false, 1, null, dataTransfer);
  srcElement.dispatchEvent(event);
  
  // drag over
  for (let i = 4; i >= 0; i--)
    EventUtils.synthesizeMouse(
      srcElement,  Math.round(offsetX/5),  Math.round(offsetY/4), 
      { type: "mousemove" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragover", true, true, contentWindow, 0, 0, 0, 0, 0, 
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);
  
  // drop
  EventUtils.synthesizeMouse(srcElement, 0, 0, { type: "mouseup" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "drop", true, true, contentWindow, 0, 0, 0, 0, 0, 
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);
}
