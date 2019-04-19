function test() {
  var ids = {};
  Array.prototype.forEach.call(document.querySelectorAll("[id]"), function(node) {
    var id = node.id;
    ok(!(id in ids), id + " should be unique");
    ids[id] = null;
  });
}
