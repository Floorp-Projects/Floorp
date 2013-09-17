// |jit-test| error: ReferenceError

function ygTreeView(id) {};
function ygNode() {}
ygNode.prototype.init = function () {
    this.children = [];
}
ygTextNode.prototype = new ygNode;
function ygTextNode() {
    this.init(it.next.bind(it), StopIteration)
}
userTree = new ygTreeView("userTree")
addMenuNode(userTree)
function addMenuNode(tree) {
    new ygTextNode({}, tree.root, false)
}
