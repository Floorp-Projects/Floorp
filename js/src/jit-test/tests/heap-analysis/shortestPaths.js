// The shortestPaths function exists solely to let the fuzzers go to town and
// exercise the code paths it calls into, hence the only things to assert
// relate to the shortestPaths test function API.
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

function describe(v) {
  return v === undefined ? "(undefined)"
    : v === null ? "(null)"
    : typeof(v) === "object" ? Object.prototype.toString.call(v)
    : typeof(v);
}

function dumpPaths(results) {
  results = results.map(paths =>
    paths.map(path =>
      path.map(({predecessor, edge}) =>
        predecessor !== undefined ?
          { predecessor: describe(predecessor), edge }
          :
          { edge }
      )
    )
  );
  print(JSON.stringify(results, null, 2));
}

print("shortestPaths([Object, f, o.p], {start: this, maxNumPaths: 5})");
var paths = shortestPaths([Object, f, o.p], {start: this, maxNumPaths: 5});
dumpPaths(paths);

print();
print("shortestPaths([f], {start: o, maxNumPaths: 1})")
paths = shortestPaths([f], {start: o, maxNumPaths: 1});
dumpPaths(paths);

print();
print("shortestPaths([f], {start: this, maxNumPaths: 5})")
paths = shortestPaths([f], {start: this, maxNumPaths: 5});
dumpPaths(paths);

print();
print("shortestPaths([f], {maxNumPaths: 5})")
paths = shortestPaths([f], {maxNumPaths: 5});
dumpPaths(paths);
assertEq(paths[0].length <= 5, true, "Too many paths reported");

paths = shortestPaths([f, g]);
assertEq(paths.length, 2, "Two sets of paths expected");

paths = shortestPaths([f], {maxNumPaths: 1});
assertEq(paths[0].length, 1, "Single path expected");

var exc;

try { paths = shortestPaths(); } catch (exc) { e = ""+exc; };
assertEq(e.includes("TypeError") && e.includes("1 argument required"), true);

try { paths = shortestPaths(100, {}); } catch (exc) { e = ""+exc; };
assertEq(e, "TypeError: 100 is not an array object");

try { paths = shortestPaths([f], {start: 200}); } catch (exc) { e = ""+exc; };
assertEq(e, "TypeError: 200 is not a GC thing");
