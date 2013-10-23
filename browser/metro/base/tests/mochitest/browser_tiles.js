let doc;

function test() {
  waitForExplicitFinish();
  Task.spawn(function(){
    info(chromeRoot + "browser_tilegrid.xul");
    yield addTab(chromeRoot + "browser_tilegrid.xul");
    doc = Browser.selectedTab.browser.contentWindow.document;
  }).then(runTests);
}

function _checkIfBoundByRichGrid_Item(expected, node, idx) {
  let binding = node.ownerDocument.defaultView.getComputedStyle(node).MozBinding;
  let result = ('url("chrome://browser/content/bindings/grid.xml#richgrid-item")' == binding);
  return (result == expected);
}
let isBoundByRichGrid_Item = _checkIfBoundByRichGrid_Item.bind(this, true);
let isNotBoundByRichGrid_Item = _checkIfBoundByRichGrid_Item.bind(this, false);

gTests.push({
  desc: "richgrid binding is applied",
  run: function() {
    ok(doc, "doc got defined");

    let grid = doc.querySelector("#grid1");
    ok(grid, "#grid1 is found");
    is(typeof grid.clearSelection, "function", "#grid1 has the binding applied");
    is(grid.items.length, 2, "#grid1 has a 2 items");
    is(grid.items[0].control, grid, "#grid1 item's control points back at #grid1'");
    ok(Array.every(grid.items, isBoundByRichGrid_Item), "All items are bound by richgrid-item");
  }
});

gTests.push({
  desc: "item clicks are handled",
  run: function() {
    let grid = doc.querySelector("#grid1");
    is(typeof grid.handleItemClick, "function", "grid.handleItemClick is a function");
    let handleStub = stubMethod(grid, 'handleItemClick');
    let itemId = "grid1_item1"; // grid.items[0].getAttribute("id");

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
    let container = doc.getElementById("alayout");
    let grid = doc.querySelector("#grid_layout");

    is(typeof grid.arrangeItems, "function", "arrangeItems is a function on the grid");

    ok(grid.tileHeight, "grid has truthy tileHeight value");
    ok(grid.tileWidth, "grid has truthy tileWidth value");

    // make the container big enough for 3 rows
    container.style.height = 3 * grid.tileHeight + 20 + "px";

    // add some items
    grid.appendItem("test title", "about:blank", true);
    grid.appendItem("test title", "about:blank", true);
    grid.appendItem("test title", "about:blank", true);
    grid.appendItem("test title", "about:blank", true);
    grid.appendItem("test title", "about:blank", true);

    grid.arrangeItems();
    // they should all fit nicely in a 3x2 grid
    is(grid.rowCount, 3, "rowCount is calculated correctly for a given container height and tileheight");
    is(grid.columnCount, 2, "columnCount is calculated correctly for a given container maxWidth and tilewidth");

    // squish the available height
    // should overflow (clip) a 2x2 grid

    let under3rowsHeight = (3 * grid.tileHeight -20) + "px";
    container.style.height = under3rowsHeight;

    let arrangedPromise = waitForEvent(grid, "arranged");
    grid.arrangeItems();
    yield arrangedPromise;

    ok(true, "arranged event is fired when arrangeItems is called");
    is(grid.rowCount, 2, "rowCount is re-calculated correctly for a given container height");
  }
});

gTests.push({
  desc: "clearAll",
  run: function() {
    let grid = doc.getElementById("clearGrid");
    grid.arrangeItems();

    // grid has rows=2 so we expect at least 2 rows and 2 columns with 3 items
    is(typeof grid.clearAll, "function", "clearAll is a function on the grid");
    is(grid.itemCount, 3, "grid has 3 items initially");
    is(grid.rowCount, 2, "grid has 2 rows initially");
    is(grid.columnCount, 2, "grid has 2 cols initially");

    let arrangeSpy = spyOnMethod(grid, "arrangeItems");
    grid.clearAll();

    is(grid.itemCount, 0, "grid has 0 itemCount after clearAll");
    is(grid.items.length, 0, "grid has 0 items after clearAll");
    // now that we use slots, an empty grid may still have non-zero rows & columns

    is(arrangeSpy.callCount, 1, "arrangeItems is called once when we clearAll");
    arrangeSpy.restore();
  }
});

gTests.push({
  desc: "empty grid",
  run: function() {
    // XXX grids have minSlots and may not be ever truly empty

    let grid = doc.getElementById("emptyGrid");
    grid.arrangeItems();
    yield waitForCondition(() => !grid.isArranging);

    // grid has 2 rows, 6 slots, 0 items
    ok(grid.isBound, "binding was applied");
    is(grid.itemCount, 0, "empty grid has 0 items");
    // minSlots attr. creates unpopulated slots
    is(grid.rowCount, grid.getAttribute("rows"), "empty grid with rows-attribute has that number of rows");
    is(grid.columnCount, 3, "empty grid has expected number of columns");

    // remove rows attribute and allow space for the grid to find its own height
    // for its number of slots
    grid.removeAttribute("rows");
    grid.parentNode.style.height = 20+(grid.tileHeight*grid.minSlots)+"px";

    grid.arrangeItems();
    yield waitForCondition(() => !grid.isArranging);
    is(grid.rowCount, grid.minSlots, "empty grid has this.minSlots rows");
    is(grid.columnCount, 1, "empty grid has 1 column");
  }
});

gTests.push({
  desc: "appendItem",
  run: function() {
     // implements an appendItem with signature title, uri, returns item element
     // appendItem triggers arrangeItems
    let grid = doc.querySelector("#emptygrid");

    is(grid.itemCount, 0, "0 itemCount when empty");
    is(grid.items.length, 0, "0 items when empty");
    is(typeof grid.appendItem, "function", "appendItem is a function on the grid");

    let arrangeStub = stubMethod(grid, "arrangeItems");
    let newItem = grid.appendItem("test title", "about:blank");

    ok(newItem && grid.items[0]==newItem, "appendItem gives back the item");
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
    is(grid.items[0].getAttribute("id"), "grid2_item2", "2nd item becomes the first item");
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
    let insertedAt0 = grid.insertItemAt(0, "inserted item 0", "http://example.com/inserted0");
    let insertedAt00 = grid.insertItemAt(0, "inserted item 00", "http://example.com/inserted00");

    ok(insertedAt0 && insertedAt00, "insertItemAt gives back an item");

    is(insertedAt0.getAttribute("label"), "inserted item 0", "insertItemAt creates item with the correct label");
    is(insertedAt0.getAttribute("value"), "http://example.com/inserted0", "insertItemAt creates item with the correct url value");

    is(grid.items[0], insertedAt00, "item is inserted at the correct index");
    is(grid.children[0], insertedAt00, "first item occupies the first slot");
    is(grid.items[1], insertedAt0, "item is inserted at the correct index");
    is(grid.children[1], insertedAt0, "next item occupies the next slot");

    is(grid.items[2].getAttribute("label"), "First item", "Old first item is now at index 2");
    is(grid.items[3].getAttribute("label"), "2nd item", "Old 2nd item is now at index 3");

    is(grid.itemCount, 4, "itemCount is incremented when we insertItemAt");

    is(arrangeStub.callCount, 2, "arrangeItems is called when we insertItemAt");
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
    let removedFirst = grid.removeItem( grid.items[0] );

    is(arrangeStub.callCount, 1, "arrangeItems is called when we removeItem");

    let removed2nd = grid.removeItem( grid.items[0], true);
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

    grid.toggleItemSelection(grid.items[1]);
    ok(grid.items[1].selected, "toggleItemSelection sets truthy selected prop on previously-unselected item");
    is(grid.selectedIndex, 1, "selectedIndex is correct");

    grid.toggleItemSelection(grid.items[1]);
    ok(!grid.items[1].selected, "toggleItemSelection sets falsy selected prop on previously-selected item");
    is(grid.selectedIndex, -1, "selectedIndex reports correctly with nothing selected");

    // item selection
    grid.selectItem(grid.items[1]);
    ok(grid.items[1].selected, "Item selected property is truthy after grid.selectItem");
    ok(grid.items[1].getAttribute("selected"), "Item selected attribute is truthy after grid.selectItem");
    ok(grid.selectedItems.length, "There are selectedItems after grid.selectItem");

    // select events
    // in seltype=single mode, select is like the default action for the tile
    // (think <a>, not <select multiple>)
    let handler = {
      handleEvent: function(aEvent) {}
    };
    let handlerStub = stubMethod(handler, "handleEvent");

    grid.items[1].selected = true;

    doc.defaultView.addEventListener("select", handler, false);
    info("select listener added");

    // clearSelection
    grid.clearSelection();
    is(grid.selectedItems.length, 0, "Nothing selected when we clearSelection");
    is(grid.selectedIndex, -1, "selectedIndex resets after clearSelection");
    is(handlerStub.callCount, 0, "clearSelection should not fire a selectionchange event");

    info("calling selectItem, currently it is:" + grid.items[0].selected);
    // Note: A richgrid in seltype=single mode fires "select" events from selectItem
    grid.selectItem(grid.items[0]);
    info("calling selectItem, now it is:" + grid.items[0].selected);
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

    grid.toggleItemSelection(grid.items[1]);
    ok(grid.items[1].selected, "toggleItemSelection sets truthy selected prop on previously-unselected item");
    is(grid.selectedItems.length, 1, "1 item selected when we first toggleItemSelection");
    is(grid.selectedItems[0], grid.items[1], "the right item is selected");
    is(grid.selectedIndex, 1, "selectedIndex is correct");

    grid.toggleItemSelection(grid.items[1]);
    is(grid.selectedItems.length, 0, "Nothing selected when we toggleItemSelection again");

    // selectionchange events
    // in seltype=multiple mode, we track selected state on all items
    // (think <select multiple> not <a>)
    let handler = {
      handleEvent: function(aEvent) {}
    };
    let handlerStub = stubMethod(handler, "handleEvent");
    doc.defaultView.addEventListener("selectionchange", handler, false);
    info("selectionchange listener added");

    // clearSelection
    grid.items[0].selected=true;
    grid.items[1].selected=true;
    is(grid.selectedItems.length, 2, "Both items are selected before calling clearSelection");
    grid.clearSelection();
    is(grid.selectedItems.length, 0, "Nothing selected when we clearSelection");
    ok(!(grid.items[0].selected || grid.items[1].selected), "selected properties all falsy when we clearSelection");
    is(handlerStub.callCount, 0, "clearSelection should not fire a selectionchange event");

    info("calling toggleItemSelection, currently it is:" + grid.items[0].selected);
    // Note: A richgrid in seltype=single mode fires "select" events from selectItem
    grid.toggleItemSelection(grid.items[0]);
    info("/calling toggleItemSelection, now it is:" + grid.items[0].selected);
    yield waitForMs(0);

    is(handlerStub.callCount, 1, "selectionchange event handler was called when we selected an item");
    is(handlerStub.calledWith[0].type, "selectionchange", "handler got a selectionchange event");
    is(handlerStub.calledWith[0].target, grid, "select event had the originating grid as the target");
    handlerStub.restore();
    doc.defaultView.removeEventListener("selectionchange", handler, false);
  }
});

gTests.push({
  desc: "selectNone",
  run: function() {
    let grid = doc.querySelector("#grid-select2");

    is(typeof grid.selectNone, "function", "selectNone is a function on the grid");

    is(grid.itemCount, 2, "2 items initially");

    // selectNone should fire a selectionchange event
    let handler = {
      handleEvent: function(aEvent) {}
    };
    let handlerStub = stubMethod(handler, "handleEvent");
    doc.defaultView.addEventListener("selectionchange", handler, false);
    info("selectionchange listener added");

    grid.items[0].selected=true;
    grid.items[1].selected=true;
    is(grid.selectedItems.length, 2, "Both items are selected before calling selectNone");
    grid.selectNone();

    is(grid.selectedItems.length, 0, "Nothing selected when we selectNone");
    ok(!(grid.items[0].selected || grid.items[1].selected), "selected properties all falsy when we selectNone");

    is(handlerStub.callCount, 1, "selectionchange event handler was called when we selectNone");
    is(handlerStub.calledWith[0].type, "selectionchange", "handler got a selectionchange event");
    is(handlerStub.calledWith[0].target, grid, "selectionchange event had the originating grid as the target");
    handlerStub.restore();
    doc.defaultView.removeEventListener("selectionchange", handler, false);
  }
});

function gridSlotsSetup() {
    let grid = this.grid = doc.createElement("richgrid");
    grid.setAttribute("minSlots", 6);
    doc.documentElement.appendChild(grid);
    is(grid.ownerDocument, doc, "created grid in the expected document");
}
function gridSlotsTearDown() {
    this.grid && this.grid.parentNode.removeChild(this.grid);
}

gTests.push({
  desc: "richgrid slots init",
  setUp: gridSlotsSetup,
  run: function() {
    let grid = this.grid;
    // grid is initially populated with empty slots matching the minSlots attribute
    is(grid.children.length, 6, "minSlots slots are created");
    is(grid.itemCount, 0, "slots do not count towards itemCount");
    ok(Array.every(grid.children, (node) => node.nodeName == 'richgriditem'), "slots have nodeName richgriditem");
    ok(Array.every(grid.children, isNotBoundByRichGrid_Item), "slots aren't bound by the richgrid-item binding");
  },
  tearDown: gridSlotsTearDown
});

gTests.push({
  desc: "richgrid using slots for items",
  setUp: gridSlotsSetup, // creates grid with minSlots = num. slots = 6
  run: function() {
    let grid = this.grid;
    let numSlots = grid.getAttribute("minSlots");
    is(grid.children.length, numSlots);
    // adding items occupies those slots
    for (let idx of [0,1,2,3,4,5,6]) {
      let slot = grid.children[idx];
      let item = grid.appendItem("item "+idx, "about:mozilla");
      if (idx < numSlots) {
        is(grid.children.length, numSlots);
        is(slot, item, "The same node is reused when an item is assigned to a slot");
      } else {
        is(typeof slot, 'undefined');
        ok(item);
        is(grid.children.length, grid.itemCount);
      }
    }
  },
  tearDown: gridSlotsTearDown
});

gTests.push({
  desc: "richgrid assign and release slots",
  setUp: function(){
    info("assign and release slots setUp");
    this.grid = doc.getElementById("slots_grid");
    this.grid.scrollIntoView();
    let rect = this.grid.getBoundingClientRect();
    info("slots grid at top: " + rect.top + ", window.pageYOffset: " + doc.defaultView.pageYOffset);
  },
  run: function() {
    let grid = this.grid;
    // start with 5 of 6 slots occupied
    for (let idx of [0,1,2,3,4]) {
      let item = grid.appendItem("item "+idx, "about:mozilla");
      item.setAttribute("id", "test_item_"+idx);
    }
    is(grid.itemCount, 5);
    is(grid.children.length, 6); // see setup, where we init with 6 slots
    let firstItem = grid.items[0];

    ok(firstItem.ownerDocument, "item has ownerDocument");
    is(doc, firstItem.ownerDocument, "item's ownerDocument is the document we expect");

    is(firstItem, grid.children[0], "Item and assigned slot are one and the same");
    is(firstItem.control, grid, "Item is bound and its .control points back at the grid");

    // before releasing, the grid should be nofified of clicks on that slot
    let testWindow = grid.ownerDocument.defaultView;

    let rect = firstItem.getBoundingClientRect();
    {
      let handleStub = stubMethod(grid, 'handleItemClick');
      // send click to item and wait for next tick;
      sendElementTap(testWindow, firstItem);
      yield waitForMs(0);

      is(handleStub.callCount, 1, "handleItemClick was called when we clicked an item");
      handleStub.restore();
    }
    // _releaseSlot is semi-private, we don't expect consumers of the binding to call it
    // but want to be sure it does what we expect
    grid._releaseSlot(firstItem);

    is(grid.itemCount, 4, "Releasing a slot gives us one less item");
    is(firstItem, grid.children[0],"Released slot is still the same node we started with");

    // after releasing, the grid should NOT be nofified of clicks
    {
      let handleStub = stubMethod(grid, 'handleItemClick');
      // send click to item and wait for next tick;
      sendElementTap(testWindow, firstItem);
      yield waitForMs(0);

      is(handleStub.callCount, 0, "handleItemClick was NOT called when we clicked a released slot");
      handleStub.restore();
    }

    ok(!firstItem.mozMatchesSelector("richgriditem[value]"), "Released slot doesn't match binding selector");
    ok(isNotBoundByRichGrid_Item(firstItem), "Released slot is no longer bound");

    waitForCondition(() => isNotBoundByRichGrid_Item(firstItem));
    ok(true, "Slot eventually gets unbound");
    is(firstItem, grid.children[0], "Released slot is still at expected index in children collection");

    let firstSlot = grid.children[0];
    firstItem = grid.insertItemAt(0, "New item 0", "about:blank");
    ok(firstItem == grid.items[0], "insertItemAt 0 creates item at expected index");
    ok(firstItem == firstSlot, "insertItemAt occupies the released slot with the new item");
    is(grid.itemCount, 5);
    is(grid.children.length, 6);
    is(firstItem.control, grid,"Item is bound and its .control points back at the grid");

    let nextSlotIndex = grid.itemCount;
    let lastItem = grid.insertItemAt(9, "New item 9", "about:blank");
    // Check we don't create sparse collection of items
    is(lastItem, grid.children[nextSlotIndex], "Item is appended at the next index when an out of bounds index is provided");
    is(grid.children.length, 6);
    is(grid.itemCount, 6);

    grid.appendItem("one more", "about:blank");
    is(grid.children.length, 7);
    is(grid.itemCount, 7);

    // clearAll results in slots being emptied
    grid.clearAll();
    is(grid.children.length, 6, "Extra slots are trimmed when we clearAll");
    ok(!Array.some(grid.children, (node) => node.hasAttribute("value")), "All slots have no value attribute after clearAll")
  },
  tearDown: gridSlotsTearDown
});

gTests.push({
  desc: "richgrid slot management",
  setUp: gridSlotsSetup,
  run: function() {
    let grid = this.grid;
    // populate grid with some items
    let numSlots = grid.getAttribute("minSlots");
    for (let idx of [0,1,2,3,4,5]) {
      let item = grid.appendItem("item "+idx, "about:mozilla");
    }

    is(grid.itemCount, 6, "Grid setup with 6 items");
    is(grid.children.length, 6, "Full grid has the expected number of slots");

    // removing an item creates a replacement slot *on the end of the stack*
    let item = grid.removeItemAt(0);
    is(item.getAttribute("label"), "item 0", "removeItemAt gives back the populated node");
    is(grid.children.length, 6);
    is(grid.itemCount, 5);
    is(grid.items[0].getAttribute("label"), "item 1", "removeItemAt removes the node so the nextSibling takes its place");
    ok(grid.children[5] && !grid.children[5].hasAttribute("value"), "empty slot is added at the end of the existing children");

    let item1 = grid.removeItem(grid.items[0]);
    is(grid.children.length, 6);
    is(grid.itemCount, 4);
    is(grid.items[0].getAttribute("label"), "item 2", "removeItem removes the node so the nextSibling takes its place");
  },
  tearDown: gridSlotsTearDown
});
