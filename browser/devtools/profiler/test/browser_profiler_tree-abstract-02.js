/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the abstract tree base class for the profiler's tree view
 * has a functional public API.
 */

let { AbstractTreeItem } = Cu.import("resource:///modules/devtools/AbstractTreeItem.jsm", {});
let { Heritage } = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});

let test = Task.async(function*() {
  let container = document.createElement("vbox");
  gBrowser.selectedBrowser.parentNode.appendChild(container);

  // Populate the tree and test `expand`, `collapse` and `getChild`...

  let treeRoot = new MyCustomTreeItem(gDataSrc, { parent: null });
  treeRoot.attachTo(container);

  ok(!treeRoot.expanded,
    "The root node should not be expanded yet.");
  ok(!treeRoot.populated,
    "The root node should not be populated yet.");

  treeRoot.expand();
  ok(treeRoot.expanded,
    "The root node should now be expanded.");
  ok(treeRoot.populated,
    "The root node should now be populated.");

  let fooItem = treeRoot.getChild(0);
  let barItem = treeRoot.getChild(1);
  ok(!fooItem.expanded && !barItem.expanded,
    "The 'foo' and 'bar' nodes should not be expanded yet.");
  ok(!fooItem.populated && !barItem.populated,
    "The 'foo' and 'bar' nodes should not be populated yet.");

  fooItem.expand();
  barItem.expand();
  ok(fooItem.expanded && barItem.expanded,
    "The 'foo' and 'bar' nodes should now be expanded.");
  ok(!fooItem.populated,
    "The 'foo' node should not be populated because it's empty.");
  ok(barItem.populated,
    "The 'bar' node should now be populated.");

  let bazItem = barItem.getChild(0);
  ok(!bazItem.expanded,
    "The 'bar' node should not be expanded yet.");
  ok(!bazItem.populated,
    "The 'bar' node should not be populated yet.");

  bazItem.expand();
  ok(bazItem.expanded,
    "The 'baz' node should now be expanded.");
  ok(!bazItem.populated,
    "The 'baz' node should not be populated because it's empty.");

  ok(!treeRoot.getChild(-1) && !treeRoot.getChild(2),
    "Calling `getChild` with out of bounds indices will return null (1).");
  ok(!fooItem.getChild(-1) && !fooItem.getChild(0),
    "Calling `getChild` with out of bounds indices will return null (2).");
  ok(!barItem.getChild(-1) && !barItem.getChild(1),
    "Calling `getChild` with out of bounds indices will return null (3).");
  ok(!bazItem.getChild(-1) && !bazItem.getChild(0),
    "Calling `getChild` with out of bounds indices will return null (4).");

  // Finished expanding all nodes in the tree...
  // Continue checking.

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

  treeRoot.collapse();
  is(container.childNodes.length, 1,
    "The container node should now have one children available.");

  ok(!treeRoot.expanded,
    "The root node should not be expanded anymore.");
  ok(fooItem.expanded && barItem.expanded && bazItem.expanded,
    "The 'foo', 'bar' and 'baz' nodes should still be expanded.");
  ok(treeRoot.populated && barItem.populated,
    "The root and 'bar' nodes should still be populated.");
  ok(!fooItem.populated && !bazItem.populated,
    "The 'foo' and 'baz' nodes should still not be populated because they're empty.");

  treeRoot.expand();
  is(container.childNodes.length, 4,
    "The container node should now have four children available again.");

  ok(treeRoot.expanded && fooItem.expanded && barItem.expanded && bazItem.expanded,
    "The root, 'foo', 'bar' and 'baz' nodes should now be reexpanded.");
  ok(treeRoot.populated && barItem.populated,
    "The root and 'bar' nodes should still be populated.");
  ok(!fooItem.populated && !bazItem.populated,
    "The 'foo' and 'baz' nodes should still not be populated because they're empty.");

  // Test `focus` on the root node...

  treeRoot.focus();
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is now focused.");

  // Test `focus` on a leaf node...

  bazItem.focus();
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is now focused.");

  // Test `remove`...

  barItem.remove();
  is(container.childNodes.length, 2,
    "The container node should now have two children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node should be the first in the container node.");
  is(container.childNodes[1], fooItem.target,
    "The 'foo' node should be the second in the container node.");

  fooItem.remove();
  is(container.childNodes.length, 1,
    "The container node should now have one children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node should be the only in the container node.");

  treeRoot.remove();
  is(container.childNodes.length, 0,
    "The container node should now have no children available.");

  container.remove();
  finish();
});

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
