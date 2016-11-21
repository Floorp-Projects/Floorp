// |jit-test| slow; need-for-each

// Binary: cache/js-dbg-32-e5e66f40c35b-linux
// Flags:
//

gczeal(2);
function subset(list, size) {
  if (size == 0 || !list.length)
    return [list.slice(0, 0)];
  var result = [];
  for (var i = 0, n = list.length; i < n; i++) {
    var pick = list.slice(i, i+1);
    var rest = list.slice(0, i).concat(list.slice(i+1));
    for each (var x in subset(rest, size-1))
      result.push(pick.concat(x));
  }
  return result;
}
var bops = [
  ["=", "|=", "^=", "&=", "<<=", ">>=", ">>>=", "+=", "-=", "*=", "/=", "%="],
  ];
var aops = [];
for (var i = 0; i < bops.length; i++) {
  for (var j = 0; j < bops[i].length; j++) {
    var k = bops[i][j];
    aops.push(k);
}
for (i = 2; i < 5; i++) {
  var sets = subset(aops, i);
  }
}
