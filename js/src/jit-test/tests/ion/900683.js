if (typeof ParallelArray === "undefined")
  quit();

ParallelArray(11701, function() {
    return /x/
}).reduce(function(a) {
    if (a % 9) {
        for (var y = 0; y; ++y) {}
        return []
    }
})

function foo() {
  return "foo";
}

function test() {
var a = [1, 2, 3];
var s = '';
for (var x of a)
  for (var i of 'y')
    s += '' + foo()
} test();

ignoreComments = [];

function bug909276() {
var actual = '';
for (var next of ignoreComments) {
  actual += a;
  for (var b in x) {
    actual += b.eval("args = [-0, NaN, -1/0]; this.f(-0, NaN, -1/0);");
  }
}
var y = Iterator([1,2,3], true);
for (var c in y) {}
} bug909276();
