// The shortestPaths function exists solely to let the fuzzers go to town and
// exercise the code paths it calls into, hence there is nothing to assert here.
//
// The actual behavior of JS::ubi::ShortestPaths is tested in
// js/src/jsapi-tests/testUbiNode.cpp, where we can actually control the
// structure of the heap graph to test specific shapes.

function f(x) {
  return x + x;
}

var g = f.bind(null, 5);

var o = {
  p: g
};

function dumpPaths(results) {
  results = results.map(paths => {
    return paths.map(path => {
      return path.map(part => {
        return {
          predecessor: Object.prototype.toString.call(part.predecessor),
          edge: part.edge
        };
      });
    });
  });
  print(JSON.stringify(results, null, 2));
}

print("shortestPaths(this, [Object, f, o.p], 5)");
var paths = shortestPaths(this, [Object, f, o.p], 5);
dumpPaths(paths);

print();
print("shortestPaths(o, [f], 1)")
paths = shortestPaths(o, [f], 1);
dumpPaths(paths);

print();
print("shortestPaths(this, [f], 5)")
paths = shortestPaths(this, [f], 5);
dumpPaths(paths);
