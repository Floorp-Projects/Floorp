/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the treeview expander arrow doesn't react to dblclick events.
 */

let { AbstractTreeItem } = Cu.import("resource:///modules/devtools/AbstractTreeItem.jsm", {});
let { Heritage } = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});

let test = Task.async(function*() {
  let container = document.createElement("vbox");
  gBrowser.selectedBrowser.parentNode.appendChild(container);

  // Populate the tree and test the root item...

  let treeRoot = new MyCustomTreeItem(gDataSrc, { parent: null });
  treeRoot.attachTo(container);

  let originalTreeRootExpanded = treeRoot.expanded;
  info("Double clicking on the root item arrow and waiting for focus event.");
  let receivedFocusEvent = treeRoot.once("focus");
  EventUtils.sendMouseEvent({ type: "dblclick" }, treeRoot.target.querySelector(".arrow"));

  yield receivedFocusEvent;
  is(treeRoot.expanded, originalTreeRootExpanded,
    "A double click on the arrow was ignored.");

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
