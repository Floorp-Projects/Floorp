// |jit-test| skip-if: !getBuildConfiguration()['moz-memory']
// Run this test only if we're using jemalloc. Other malloc implementations
// exhibit surprising behaviors. For example, 32-bit Fedora builds have
// non-deterministic allocation sizes.

// Check that JS::ubi::Node::size returns reasonable results for objects.

// We actually hard-code specific sizes into this test, even though they're
// implementation details, because in practice there are only two architecture
// variants to consider (32-bit and 64-bit), and if these sizes change, that's
// something SpiderMonkey hackers really want to know; they're supposed to be
// stable.

if (getBuildConfiguration()['pointer-byte-size'] == 4)
  var s = (s32, s64) => s32
else
  var s = (s32, s64) => s64

function tenure(obj) {
  gc();
  return obj;
}

// Return the byte size of |obj|, ensuring that the size is not affected by
// being tenured. (We use 'survives a GC' as an approximation for 'tenuring'.)
function tByteSize(obj) {
  var size = byteSize(obj);
  minorgc();
  if (size != byteSize(obj))
    return 0;
  return size;
}

assertEq(tByteSize({}),                                 s(48,  56));

// Try objects with only named properties.
assertEq(tByteSize({ w: 1 }),                           s(32,  40));
assertEq(tByteSize({ w: 1, x: 2 }),                     s(32,  40));
assertEq(tByteSize({ w: 1, x: 2, y: 3 }),               s(48,  56));
assertEq(tByteSize({ w: 1, x: 2, y: 3, z:4 }),          s(48,  56));
assertEq(tByteSize({ w: 1, x: 2, y: 3, z:4, a: 5 }),    s(80,  88));

// Try objects with only indexed properties.
assertEq(tByteSize({ 0:0 }),                            s(96,  104));
assertEq(tByteSize({ 0:0, 1:1 }),                       s(96,  104));
assertEq(tByteSize({ 0:0, 1:1, 2:2 }),                  s(112, 120));
assertEq(tByteSize({ 0:0, 1:1, 2:2, 3:3 }),             s(112, 120));
assertEq(tByteSize({ 0:0, 1:1, 2:2, 3:3, 4:4 }),        s(144, 152));

// Mix indexed and named properties, exploring each combination of the size
// classes above.
//
// Oddly, the changes here as the objects grow are not simply the sums of the
// changes above: for example, with one named property, the objects with three
// and five indexed properties are in different size classes; but with three
// named properties, there's no break there.
assertEq(tByteSize({ w:1,                     0:0                     }),  s(96,  104));
assertEq(tByteSize({ w:1,                     0:0, 1:1, 2:2           }),  s(112, 120));
assertEq(tByteSize({ w:1,                     0:0, 1:1, 2:2, 3:3, 4:4 }),  s(144, 152));
assertEq(tByteSize({ w:1, x:2, y:3,           0:0                     }),  s(112, 120));
assertEq(tByteSize({ w:1, x:2, y:3,           0:0, 1:1, 2:2           }),  s(144, 152));
assertEq(tByteSize({ w:1, x:2, y:3,           0:0, 1:1, 2:2, 3:3, 4:4 }),  s(144, 152));
assertEq(tByteSize({ w:1, x:2, y:3, z:4, a:6, 0:0                     }),  s(144, 152));
assertEq(tByteSize({ w:1, x:2, y:3, z:4, a:6, 0:0, 1:1, 2:2           }),  s(144, 152));
assertEq(tByteSize({ w:1, x:2, y:3, z:4, a:6, 0:0, 1:1, 2:2, 3:3, 4:4 }),  s(176, 184));

// Check various lengths of array.
assertEq(tByteSize([]),                                 s(80,  88));
assertEq(tByteSize([1]),                                s(48,  56));
assertEq(tByteSize([1, 2]),                             s(48,  56));
assertEq(tByteSize([1, 2, 3]),                          s(80,  88));
assertEq(tByteSize([1, 2, 3, 4]),                       s(80,  88));
assertEq(tByteSize([1, 2, 3, 4, 5]),                    s(80,  88));
assertEq(tByteSize([1, 2, 3, 4, 5, 6]),                 s(80,  88));
assertEq(tByteSize([1, 2, 3, 4, 5, 6, 7]),              s(112, 120));
assertEq(tByteSize([1, 2, 3, 4, 5, 6, 7, 8]),           s(112, 120));

// Various forms of functions.
assertEq(tByteSize(function () {}),                     s(40,  56));
assertEq(tByteSize(function () {}.bind()),              s(56,  72));
assertEq(tByteSize(() => 1),                            s(56,  72));
assertEq(tByteSize(Math.sin),                           s(40,  56));
