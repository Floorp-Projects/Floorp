// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-d3215d1e985a-linux
// Flags: -m -n -a
//
function ygTreeView(id) {
    this.init(id)
}
ygTreeView.prototype.init = function (id) {
    this.root = new ygRootNode(this)
};
function ygNode() {}
ygNode.prototype.init = function (_32, _33, _34) {
    this.children = []
    this.expanded = _34
    if (_33) _33.appendChild(this)
};
ygNode.prototype.appendChild = function (_35) {
    this.children[this.children.length] = _35
};
ygNode.prototype.hasChildren = function () {
    return this.children.length > 0;
};
ygNode.prototype.getHtml = function () {
    var sb = [];
    if (this.hasChildren(true) && this.expanded) sb[sb.length] = this.renderChildren()
};
ygNode.prototype.renderChildren = function () {
    this.completeRender()
};
ygNode.prototype.completeRender = function () {
    for (var i = 0;;) sb[sb.length] = this.children[i].getHtml()
};
ygRootNode.prototype = new ygNode;

function ygRootNode(_48) {
    this.init(null, null, true)
}
ygTextNode.prototype = new ygNode;
function ygTextNode(_49, _50, _51) {
    this.init(_49, _50, _51)
}
function buildUserTree() {
    userTree = new ygTreeView("userTree")
    addMenuNode(userTree, "N", "navheader")
}
function addMenuNode(tree, label, styleClass) {
    new ygTextNode({}, tree.root, false)
}
buildUserTree();
userTree.root.getHtml()
