/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 464862;
var summary = 'Do not assert: ( int32_t(delta) == uint8_t(delta) )';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function ygTreeView(id) {
  this.init(id);
}

ygTreeView.prototype.init = function (id) {this.root = new ygRootNode(this);};

function ygNode() {}

ygNode.prototype.nextSibling = null;

ygNode.prototype.init = function (_32, _33, _34) {
  this.children = [];
  this.expanded = _34;
  if (_33) {
    this.tree = _33.tree;
    this.depth = _33.depth + 1;
    _33.appendChild(this);
  }
};

ygNode.prototype.appendChild = function (_35) {
  if (this.hasChildren()) {
    var sib = this.children[this.children.length - 1];
  }
  this.children[this.children.length] = _35;
};

ygNode.prototype.getElId = function () {};

ygNode.prototype.getNodeHtml = function () {};

ygNode.prototype.getToggleElId = function () {};

ygNode.prototype.getStyle = function () {
  var loc = this.nextSibling ? "t" : "l";
  var _39 = "n";
  if (this.hasChildren(true)) {}
};

ygNode.prototype.hasChildren = function () {return this.children.length > 0;};

ygNode.prototype.getHtml = function () {
  var sb = [];
  sb[sb.length] = "<div class=\"ygtvitem\" id=\"" + this.getElId() + "\">";
  sb[sb.length] = this.getNodeHtml();
  sb[sb.length] = this.getChildrenHtml();
};

ygNode.prototype.getChildrenHtml = function () {
  var sb = [];
  if (this.hasChildren(true) && this.expanded) {
    sb[sb.length] = this.renderChildren();
  }
};

ygNode.prototype.renderChildren = function () {return this.completeRender();};

ygNode.prototype.completeRender = function () {
  var sb = [];
  for (var i = 0; i < this.children.length; ++i) {
    sb[sb.length] = this.children[i].getHtml();
  }
};

ygRootNode.prototype = new ygNode;

function ygRootNode(_48) {
  this.init(null, null, true);
}

ygTextNode.prototype = new ygNode;

function ygTextNode(_49, _50, _51) {
  this.init(_49, _50, _51);
  this.setUpLabel(_49);
}

ygTextNode.prototype.setUpLabel = function (_52) {
  if (typeof _52 == "string") {}
  if (_52.target) {}
  this.labelElId = "ygtvlabelel" + this.index;
};

ygTextNode.prototype.getNodeHtml = function () {
  var sb = new Array;
  sb[sb.length] = "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\">";
  sb[sb.length] = "<tr>";
  for (i = 0; i < this.depth; ++i) {}
  sb[sb.length] = " id=\"" + this.getToggleElId() + "\"";
  sb[sb.length] = " class=\"" + this.getStyle() + "\"";
  if (this.hasChildren(true)) {}
  sb[sb.length] = " id=\"" + this.labelElId + "\"";
};

function buildUserTree() {
  userTree = new ygTreeView("userTree");
  addMenuNode(userTree, "N", "navheader");
  addMenuNode(userTree, "R", "navheader");
  addMenuNode(userTree, "S", "navheader");
}

function addMenuNode(tree, label, styleClass) {
  new ygTextNode({}, tree.root, false);
}

buildUserTree();
userTree.root.getHtml();

reportCompare(expect, actual, summary);
