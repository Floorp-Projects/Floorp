var src = `
var p = {y: 1};
var arr = [];
for (var i = 0; i < 10; i++) {
  var o = Object.create(p);
  o["x" + i] = 2;
  arr.push(o);
}
arr
`;

var wrapper = evaluate(src, {global: newGlobal({sameZoneAs: this})});
for (var i = 0; i < 50; i++) {
  assertEq(wrapper[i % 10].y, 1);
}
