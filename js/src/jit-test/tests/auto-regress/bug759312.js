// Binary: cache/js-dbg-32-4ce3983a43f4-linux
// Flags: --ion-eager
//
try {
  gczeal(2);
var MyMath = {
  random: function() {
    this.seed = ((this.seed + 0x7ed55d16) + (this.seed << 12))  & 0xffffffff;
    return (this.seed & 0xfffffff) / 0x10000000;
  },
};
var kSplayTreeSize = 8000;
var kSplayTreePayloadDepth = 5;
function GeneratePayloadTree(depth, key) {
  if (depth == 0) {
    return {
      array  : [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
      string : 'String for key ' + key + ' in leaf node'
    };
  } else {
    return {
      left:  GeneratePayloadTree(depth - 1, key),
      right: GeneratePayloadTree(depth - 1, key)
    };
  }
}
function GenerateKey() {
  return MyMath.random();
}
function InsertNewNode() {
  do {
    key = GenerateKey();
  } while (splayTree.find(key) != null);
  splayTree.insert(key, GeneratePayloadTree(kSplayTreePayloadDepth, key));
}
function SplaySetup() {
  splayTree = new SplayTree();
  for (var i = 0; i < kSplayTreeSize; i++) InsertNewNode();
}
function SplayTree() {
};
SplayTree.prototype.isEmpty = function() {
  return !this.root_;
};
SplayTree.prototype.insert = function(key, value) {
  if (this.isEmpty()) {
    this.root_ = new SplayTree.Node(key, value);
  }
  var node = new SplayTree.Node(key, value);
  if (key > this.root_.key) {
    this.root_.left = null;
  }
  this.root_ = node;
};
SplayTree.prototype.find = function(key) {};
SplayTree.Node = function(key, value) {
  this.key = key;
};
SplaySetup();
} catch(exc1) {}
