/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the abstract tree base class for the profiler's tree view
 * works as advertised.
 */

let { AbstractTreeItem } = Cu.import("resource:///modules/devtools/AbstractTreeItem.jsm", {});
let { Heritage } = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});

function* spawnTest() {
  let container = document.createElement("vbox");
  gBrowser.selectedBrowser.parentNode.appendChild(container);

  // Populate the tree and test the root item...

  let treeRoot = new MyCustomTreeItem(gDataSrc, { parent: null });
  treeRoot.attachTo(container);

  ok(!treeRoot.expanded,
    "The root node should not be expanded yet.");
  ok(!treeRoot.populated,
    "The root node should not be populated yet.");

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");

  is(treeRoot.root, treeRoot,
    "The root node has the correct root.");
  is(treeRoot.parent, null,
    "The root node has the correct parent.");
  is(treeRoot.level, 0,
    "The root node has the correct level.");
  is(treeRoot.target.MozMarginStart, "0px",
    "The root node's indentation is correct.");
  is(treeRoot.target.textContent, "root",
    "The root node's text contents are correct.");
  is(treeRoot.container, container,
    "The root node's container is correct.");

  // Expand the root and test the child items...

  let receivedExpandEvent = treeRoot.once("expand");
  EventUtils.sendMouseEvent({ type: "mousedown" }, treeRoot.target.querySelector(".arrow"));

  let eventItem = yield receivedExpandEvent;
  is(eventItem, treeRoot,
    "The 'expand' event target is correct.");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is now focused.");

  let fooItem = treeRoot.getChild(0);
  let barItem = treeRoot.getChild(1);

  is(container.childNodes.length, 3,
    "The container node should now have three children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");
  is(container.childNodes[1], fooItem.target,
    "The 'foo' node's target is a child of the container node.");
  is(container.childNodes[2], barItem.target,
    "The 'bar' node's target is a child of the container node.");

  is(fooItem.root, treeRoot,
    "The 'foo' node has the correct root.");
  is(fooItem.parent, treeRoot,
    "The 'foo' node has the correct parent.");
  is(fooItem.level, 1,
    "The 'foo' node has the correct level.");
  is(fooItem.target.MozMarginStart, "10px",
    "The 'foo' node's indentation is correct.");
  is(fooItem.target.textContent, "foo",
    "The 'foo' node's text contents are correct.");
  is(fooItem.container, container,
    "The 'foo' node's container is correct.");

  is(barItem.root, treeRoot,
    "The 'bar' node has the correct root.");
  is(barItem.parent, treeRoot,
    "The 'bar' node has the correct parent.");
  is(barItem.level, 1,
    "The 'bar' node has the correct level.");
  is(barItem.target.MozMarginStart, "10px",
    "The 'bar' node's indentation is correct.");
  is(barItem.target.textContent, "bar",
    "The 'bar' node's text contents are correct.");
  is(barItem.container, container,
    "The 'bar' node's container is correct.");

  // Test other events on the child nodes...

  let receivedFocusEvent = treeRoot.once("focus");
  EventUtils.sendMouseEvent({ type: "mousedown" }, fooItem.target);

  eventItem = yield receivedFocusEvent;
  is(eventItem, fooItem,
    "The 'focus' event target is correct.");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is now focused.");

  let receivedDblClickEvent = treeRoot.once("focus");
  EventUtils.sendMouseEvent({ type: "dblclick" }, barItem.target);

  eventItem = yield receivedDblClickEvent;
  is(eventItem, barItem,
    "The 'dblclick' event target is correct.");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is now focused.");

  // A child item got expanded, test the descendants...

  let bazItem = barItem.getChild(0);

  is(container.childNodes.length, 4,
    "The container node should now have four children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");
  is(container.childNodes[1], fooItem.target,
    "The 'foo' node's target is a child of the container node.");
  is(container.childNodes[2], barItem.target,
    "The 'bar' node's target is a child of the container node.");
  is(container.childNodes[3], bazItem.target,
    "The 'baz' node's target is a child of the container node.");

  is(bazItem.root, treeRoot,
    "The 'baz' node has the correct root.");
  is(bazItem.parent, barItem,
    "The 'baz' node has the correct parent.");
  is(bazItem.level, 2,
    "The 'baz' node has the correct level.");
  is(bazItem.target.MozMarginStart, "20px",
    "The 'baz' node's indentation is correct.");
  is(bazItem.target.textContent, "baz",
    "The 'baz' node's text contents are correct.");
  is(bazItem.container, container,
    "The 'baz' node's container is correct.");

  container.remove();
  finish();
}

function MyCustomTreeItem(dataSrc, properties) {
  AbstractTreeItem.call(this, properties);
  this.itemDataSrc = dataSrc;
}

MyCustomTreeItem.prototype = Heritage.extend(AbstractTreeItem.prototype, {
  _displaySelf: function(document, arrowNode) {
    let node = document.createElement("hbox");
    node.MozMarginStart = (this.level * 10) + "px";
    node.appendChild(arrowNode);
    node.appendChild(document.createTextNode(this.itemDataSrc.label));
    return node;
  },
  _populateSelf: function(children) {
    for (let childDataSrc of this.itemDataSrc.children) {
      children.push(new MyCustomTreeItem(childDataSrc, {
        parent: this,
        level: this.level + 1
      }));
    }
  }
});

let gDataSrc = {
  label: "root",
  children: [{
    label: "foo",
    children: []
  }, {
    label: "bar",
    children: [{
      label: "baz",
      children: []
    }]
  }]
};
