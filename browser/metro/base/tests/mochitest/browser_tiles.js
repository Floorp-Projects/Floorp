let doc;

function test() {
  waitForExplicitFinish();
  Task.spawn(function(){
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_tilegrid.xul");
    yield addTab(chromeRoot + "browser_tilegrid.xul");
    doc = Browser.selectedTab.browser.contentWindow.document;
  }).then(runTests);
}

gTests.push({
  desc: "richgrid binding is applied",
  run: function() {
    ok(doc, "doc got defined");

    let grid = doc.querySelector("#grid1");
    ok(grid, "#grid1 is found");
    is(typeof grid.clearSelection, "function", "#grid1 has the binding applied");

    is(grid.children.length, 1, "#grid1 has a single item");
    is(grid.children[0].control, grid, "#grid1 item's control points back at #grid1'");
  }
});

gTests.push({
  desc: "item clicks are handled",
  run: function() {
    let grid = doc.querySelector("#grid1");
    is(typeof grid.handleItemClick, "function", "grid.handleItemClick is a function");
    let handleStub = stubMethod(grid, 'handleItemClick');
    let itemId = "grid1_item1"; // grid.children[0].getAttribute("id");

    // send click to item and wait for next tick;
    EventUtils.sendMouseEvent({type: 'click'}, itemId, doc.defaultView);
    yield waitForMs(0);

    is(handleStub.callCount, 1, "handleItemClick was called when we clicked an item");
    handleStub.restore();

    // if the grid has a controller, it should be called too
    let gridController = {
      handleItemClick: function() {}
    };
    let controllerHandleStub = stubMethod(gridController, "handleItemClick");
    let origController = grid.controller;
    grid.controller = gridController;

    // send click to item and wait for next tick;
    EventUtils.sendMouseEvent({type: 'click'}, itemId, doc.defaultView);
    yield waitForMs(0);

    is(controllerHandleStub.callCount, 1, "controller.handleItemClick was called when we clicked an item");
    is(controllerHandleStub.calledWith[0], doc.getElementById(itemId), "controller.handleItemClick was passed the grid item");
    grid.controller = origController;
  }
});

gTests.push({
  desc: "arrangeItems",
  run: function() {
     // implements an arrangeItems method, with optional cols, rows signature
    let grid = doc.querySelector("#grid1");
    is(typeof grid.arrangeItems, "function", "arrangeItems is a function on the grid");
    todo(false, "Test outcome of arrangeItems with cols and rows arguments");
  }
});

gTests.push({
  desc: "appendItem",
  run: function() {
     // implements an appendItem with signature title, uri, returns item element
     // appendItem triggers arrangeItems
    let grid = doc.querySelector("#emptygrid");

    is(grid.itemCount, 0, "0 itemCount when empty");
    is(grid.children.length, 0, "0 children when empty");
    is(typeof grid.appendItem, "function", "appendItem is a function on the grid");

    let arrangeStub = stubMethod(grid, "arrangeItems");
    let newItem = grid.appendItem("test title", "about:blank");

    ok(newItem && grid.children[0]==newItem, "appendItem gives back the item");
    is(grid.itemCount, 1, "itemCount is incremented when we appendItem");
    is(newItem.getAttribute("label"), "test title", "title ends up on label attribute");
    is(newItem.getAttribute("value"), "about:blank", "url ends up on value attribute");

    is(arrangeStub.callCount, 1, "arrangeItems is called when we appendItem");
    arrangeStub.restore();
  }
});

gTests.push({
  desc: "getItemAtIndex",
  run: function() {
     // implements a getItemAtIndex method
    let grid = doc.querySelector("#grid2");
    is(typeof grid.getItemAtIndex, "function", "getItemAtIndex is a function on the grid");
    is(grid.getItemAtIndex(0).getAttribute("id"), "grid2_item1", "getItemAtIndex retrieves the first item");
    is(grid.getItemAtIndex(1).getAttribute("id"), "grid2_item2", "getItemAtIndex item at index 2");
    ok(!grid.getItemAtIndex(5), "getItemAtIndex out-of-bounds index returns falsy");
  }
});

gTests.push({
  desc: "removeItemAt",
  run: function() {
     // implements a removeItemAt method, with 'index' signature
     // removeItemAt triggers arrangeItems
    let grid = doc.querySelector("#grid2");

    is(grid.itemCount, 2, "2 items initially");
    is(typeof grid.removeItemAt, "function", "removeItemAt is a function on the grid");

    let arrangeStub = stubMethod(grid, "arrangeItems");
    let removedItem = grid.removeItemAt(0);

    ok(removedItem, "removeItemAt gives back an item");
    is(removedItem.getAttribute("id"), "grid2_item1", "removeItemAt gives back the correct item");
    is(grid.children[0].getAttribute("id"), "grid2_item2", "2nd item becomes the first item");
    is(grid.itemCount, 1, "itemCount is decremented when we removeItemAt");

    is(arrangeStub.callCount, 1, "arrangeItems is called when we removeItemAt");
    arrangeStub.restore();
  }
});

gTests.push({
  desc: "insertItemAt",
  run: function() {
     // implements an insertItemAt method, with index, title, uri.spec signature
     // insertItemAt triggers arrangeItems
    let grid = doc.querySelector("#grid3");

    is(grid.itemCount, 2, "2 items initially");
    is(typeof grid.insertItemAt, "function", "insertItemAt is a function on the grid");

    let arrangeStub = stubMethod(grid, "arrangeItems");
    let insertedItem = grid.insertItemAt(1, "inserted item", "http://example.com/inserted");

    ok(insertedItem, "insertItemAt gives back an item");
    is(grid.children[1], insertedItem, "item is inserted at the correct index");
    is(insertedItem.getAttribute("label"), "inserted item", "insertItemAt creates item with the correct label");
    is(insertedItem.getAttribute("value"), "http://example.com/inserted", "insertItemAt creates item with the correct url value");
    is(grid.children[2].getAttribute("id"), "grid3_item2", "following item ends up at the correct index");
    is(grid.itemCount, 3, "itemCount is incremented when we insertItemAt");

    is(arrangeStub.callCount, 1, "arrangeItems is called when we insertItemAt");
    arrangeStub.restore();
  }
});

gTests.push({
  desc: "getIndexOfItem",
  run: function() {
     // implements a getIndexOfItem method, with item (element) signature
     // insertItemAt triggers arrangeItems
    let grid = doc.querySelector("#grid4");

    is(grid.itemCount, 2, "2 items initially");
    is(typeof grid.getIndexOfItem, "function", "getIndexOfItem is a function on the grid");

    let item = doc.getElementById("grid4_item2");
    let badItem = doc.createElement("richgriditem");

    is(grid.getIndexOfItem(item), 1, "getIndexOfItem returns the correct value for an item");
    is(grid.getIndexOfItem(badItem), -1, "getIndexOfItem returns -1 for items it doesn't contain");
  }
});

gTests.push({
  desc: "getItemsByUrl",
  run: function() {
    let grid = doc.querySelector("#grid5");

    is(grid.itemCount, 4, "4 items total");
    is(typeof grid.getItemsByUrl, "function", "getItemsByUrl is a function on the grid");

    ['about:blank', 'http://bugzilla.mozilla.org/'].forEach(function(testUrl) {
      let items = grid.getItemsByUrl(testUrl);
      is(items.length, 2, "2 matching items in the test grid");
      is(items.item(0).url, testUrl, "Matched item has correct url property");
      is(items.item(1).url, testUrl, "Matched item has correct url property");
    });

    let badUrl = 'http://gopher.well.com:70/';
    let items = grid.getItemsByUrl(badUrl);
    is(items.length, 0, "0 items matched url: "+badUrl);

  }
});

gTests.push({
  desc: "removeItem",
  run: function() {
    let grid = doc.querySelector("#grid5");

    is(grid.itemCount, 4, "4 items total");
    is(typeof grid.removeItem, "function", "removeItem is a function on the grid");

    let arrangeStub = stubMethod(grid, "arrangeItems");
    let removedFirst = grid.removeItem( grid.children[0] );

    is(arrangeStub.callCount, 1, "arrangeItems is called when we removeItem");

    let removed2nd = grid.removeItem( grid.children[0], true);
    is(removed2nd.getAttribute("label"), "2nd item", "the next item was returned");
    is(grid.itemCount, 2, "2 items remain");

    // callCount should still be at 1
    is(arrangeStub.callCount, 1, "arrangeItems is not called when we pass the truthy skipArrange param");

    let otherItem = grid.ownerDocument.querySelector("#grid6_item1");
    let removedFail = grid.removeItem(otherItem);
    ok(!removedFail, "Falsy value returned when non-child item passed");
    is(grid.itemCount, 2, "2 items remain");

    // callCount should still be at 1
    is(arrangeStub.callCount, 1, "arrangeItems is not called when nothing is matched");

    arrangeStub.restore();
  }
});

gTests.push({
  desc: "selections (single)",
  run: function() {
     // when seltype is single,
     //      maintains a selectedItem property
     //      maintains a selectedIndex property
     //     clearSelection, selectItem, toggleItemSelection methods are implemented
     //     'select' events are implemented
    let grid = doc.querySelector("#grid-select1");

    is(typeof grid.clearSelection, "function", "clearSelection is a function on the grid");
    is(typeof grid.selectedItems, "object", "selectedItems is a property on the grid");
    is(typeof grid.toggleItemSelection, "function", "toggleItemSelection is function on the grid");
    is(typeof grid.selectItem, "function", "selectItem is a function on the grid");

    is(grid.itemCount, 2, "2 items initially");
    is(grid.selectedItems.length, 0, "nothing selected initially");

    grid.toggleItemSelection(grid.children[1]);
    ok(grid.children[1].selected, "toggleItemSelection sets truthy selected prop on previously-unselected item");
    is(grid.selectedIndex, 1, "selectedIndex is correct");

    grid.toggleItemSelection(grid.children[1]);
    ok(!grid.children[1].selected, "toggleItemSelection sets falsy selected prop on previously-selected item");
    is(grid.selectedIndex, -1, "selectedIndex reports correctly with nothing selected");

    // item selection
    grid.selectItem(grid.children[1]);
    ok(grid.children[1].selected, "Item selected property is truthy after grid.selectItem");
    ok(grid.children[1].getAttribute("selected"), "Item selected attribute is truthy after grid.selectItem");
    ok(grid.selectedItems.length, "There are selectedItems after grid.selectItem");

    // clearSelection
    grid.selectItem(grid.children[0]);
    grid.selectItem(grid.children[1]);
    grid.clearSelection();
    is(grid.selectedItems.length, 0, "Nothing selected when we clearSelection");
    is(grid.selectedIndex, -1, "selectedIndex resets after clearSelection");

    // select events
    // in seltype=single mode, select is like the default action for the tile
    // (think <a>, not <select multiple>)
    let handler = {
      handleEvent: function(aEvent) {}
    };
    let handlerStub = stubMethod(handler, "handleEvent");
    doc.defaultView.addEventListener("select", handler, false);
    info("select listener added");

    info("calling selectItem, currently it is:" + grid.children[0].selected);
    // Note: A richgrid in seltype=single mode fires "select" events from selectItem
    grid.selectItem(grid.children[0]);
    info("calling selectItem, now it is:" + grid.children[0].selected);
    yield waitForMs(0);

    is(handlerStub.callCount, 1, "select event handler was called when we selected an item");
    is(handlerStub.calledWith[0].type, "select", "handler got a select event");
    is(handlerStub.calledWith[0].target, grid, "select event had the originating grid as the target");
    handlerStub.restore();
    doc.defaultView.removeEventListener("select", handler, false);
  }
});

gTests.push({
  desc: "selections (multiple)",
  run: function() {
     // when seltype is multiple,
     //      maintains a selectedItems property
     //      clearSelection, selectItem, toggleItemSelection methods are implemented
     //     'selectionchange' events are implemented
    let grid = doc.querySelector("#grid-select2");

    is(typeof grid.clearSelection, "function", "clearSelection is a function on the grid");
    is(typeof grid.selectedItems, "object", "selectedItems is a property on the grid");
    is(typeof grid.toggleItemSelection, "function", "toggleItemSelection is function on the grid");
    is(typeof grid.selectItem, "function", "selectItem is a function on the grid");

    is(grid.itemCount, 2, "2 items initially");
    is(grid.selectedItems.length, 0, "nothing selected initially");

    grid.toggleItemSelection(grid.children[1]);
    ok(grid.children[1].selected, "toggleItemSelection sets truthy selected prop on previously-unselected item");
    is(grid.selectedItems.length, 1, "1 item selected when we first toggleItemSelection");
    is(grid.selectedItems[0], grid.children[1], "the right item is selected");
    is(grid.selectedIndex, 1, "selectedIndex is correct");

    grid.toggleItemSelection(grid.children[1]);
    is(grid.selectedItems.length, 0, "Nothing selected when we toggleItemSelection again");

    // clearSelection
    grid.children[0].selected=true;
    grid.children[1].selected=true;
    is(grid.selectedItems.length, 2, "Both items are selected before calling clearSelection");
    grid.clearSelection();
    is(grid.selectedItems.length, 0, "Nothing selected when we clearSelection");
    ok(!(grid.children[0].selected || grid.children[1].selected), "selected properties all falsy when we clearSelection");

    // selectionchange events
    // in seltype=multiple mode, we track selected state on all items
    // (think <select multiple> not <a>)
    let handler = {
      handleEvent: function(aEvent) {}
    };
    let handlerStub = stubMethod(handler, "handleEvent");
    doc.defaultView.addEventListener("selectionchange", handler, false);
    info("selectionchange listener added");

    info("calling toggleItemSelection, currently it is:" + grid.children[0].selected);
    // Note: A richgrid in seltype=single mode fires "select" events from selectItem
    grid.toggleItemSelection(grid.children[0]);
    info("/calling toggleItemSelection, now it is:" + grid.children[0].selected);
    yield waitForMs(0);

    is(handlerStub.callCount, 1, "selectionchange event handler was called when we selected an item");
    is(handlerStub.calledWith[0].type, "selectionchange", "handler got a selectionchange event");
    is(handlerStub.calledWith[0].target, grid, "select event had the originating grid as the target");
    handlerStub.restore();
    doc.defaultView.removeEventListener("selectionchange", handler, false);
  }
});

     // implements a getItemAtIndex method (or grid.children[idx] ?)

