// Binary: cache/js-dbg-64-8a59519e137e-linux
// Flags: -m -n -a
//

var obj = new Object();
var index = [ (null ), 1073741824, 1073741825 ];
for (var j in index) {
  testProperty(index[j]);
}
function testProperty(i) {
  actual = obj[String(i)];
}
