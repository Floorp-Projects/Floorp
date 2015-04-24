// Check JS::ubi::Node::size results for strings.

// We actually hard-code specific sizes into this test, even though they're
// implementation details, because in practice there are only two architecture
// variants to consider (32-bit and 64-bit), and if these sizes change, that's
// something SpiderMonkey hackers really want to know; they're supposed to be
// stable.

// Run this test only if we're using jemalloc. Other malloc implementations
// exhibit surprising behaviors. For example, 32-bit Fedora builds have
// non-deterministic allocation sizes.
var config = getBuildConfiguration();
if (!config['moz-memory'])
  quit(0);

if (config['pointer-byte-size'] == 4)
  var s = (s32, s64) => s32
else
  var s = (s32, s64) => s64

// Return the byte size of |obj|, ensuring that the size is not affected by
// being tenured. (We use 'survives a GC' as an approximation for 'tenuring'.)
function tByteSize(obj) {
  var nurserySize = byteSize(obj);
  minorgc();
  var tenuredSize = byteSize(obj);
  if (nurserySize != tenuredSize) {
    print("nursery size: " + nurserySize + "  tenured size: " + tenuredSize);
    return -1; // make the stack trace point at the real test
  }

  return tenuredSize;
}

// There are four representations of flat strings, with the following capacities
// (excluding a terminating null character):
//
//                      32-bit                  64-bit                test
// representation       Latin-1   char16_t      Latin-1   char16_t    label
// ========================================================================
// JSExternalString            (cannot be tested in shell)            -
// JSThinInlineString   7         3             15        7           T
// JSFatInlineString    23        11            23        11          F
// JSExtensibleString          - limited by available memory -        X
// JSUndependedString          - same as JSExtensibleString -

// Latin-1
assertEq(tByteSize(""),                                                 s(16, 24)); // T, T
assertEq(tByteSize("1"),                                                s(16, 24)); // T, T
assertEq(tByteSize("1234567"),                                          s(16, 24)); // T, T
assertEq(tByteSize("12345678"),                                         s(32, 24)); // F, T
assertEq(tByteSize("123456789.12345"),                                  s(32, 24)); // F, T
assertEq(tByteSize("123456789.123456"),                                 s(32, 32)); // F, F
assertEq(tByteSize("123456789.123456789.123"),                          s(32, 32)); // F, F
assertEq(tByteSize("123456789.123456789.1234"),                         s(48, 56)); // X, X
assertEq(tByteSize("123456789.123456789.123456789.1"),                  s(48, 56)); // X, X
assertEq(tByteSize("123456789.123456789.123456789.12"),                 s(64, 72)); // X, X

// Inline char16_t atoms.
// "Impassionate gods have never seen the red that is the Tatsuta River."
//   - Ariwara no Narihira
assertEq(tByteSize("千"),						s(16, 24)); // T, T
assertEq(tByteSize("千早"),    						s(16, 24)); // T, T
assertEq(tByteSize("千早ぶ"),    					s(16, 24)); // T, T
assertEq(tByteSize("千早ぶる"),    					s(32, 24)); // F, T
assertEq(tByteSize("千早ぶる神"),    					s(32, 24)); // F, T
assertEq(tByteSize("千早ぶる神代"),					s(32, 24)); // F, T
assertEq(tByteSize("千早ぶる神代も"),					s(32, 24)); // F, T
assertEq(tByteSize("千早ぶる神代もき"),					s(32, 32)); // F, F
assertEq(tByteSize("千早ぶる神代もきかず龍"),				s(32, 32)); // F, F
assertEq(tByteSize("千早ぶる神代もきかず龍田"),    			s(48, 56)); // X, X
assertEq(tByteSize("千早ぶる神代もきかず龍田川 か"),    			s(48, 56)); // X, X
assertEq(tByteSize("千早ぶる神代もきかず龍田川 から"),    			s(64, 72)); // X, X
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水く"),    	s(64, 72)); // X, X
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くく"),    	s(80, 88)); // X, X
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くくるとは"),	s(80, 88)); // X, X

// A Latin-1 rope. This changes size when flattened.
// "In a village of La Mancha, the name of which I have no desire to call to mind"
//   - Miguel de Cervantes, Don Quixote
var fragment8 = "En un lugar de la Mancha, de cuyo nombre no quiero acordarme"; // 60 characters
var rope8 = fragment8;
for (var i = 0; i < 10; i++) // 1024 repetitions
  rope8 = rope8 + rope8;
assertEq(tByteSize(rope8),                                              s(16, 24));
var matches8 = rope8.match(/(de cuyo nombre no quiero acordarme)/);
assertEq(tByteSize(rope8),                                              s(16 + 65536,  24 + 65536));

// Test extensible strings.
//
// Appending another copy of the fragment should yield another rope.
//
// Flatting that should turn the original rope into a dependent string, and
// yield a new linear string, of the some size as the original.
rope8a = rope8 + fragment8;
assertEq(tByteSize(rope8a),                                             s(16, 24));
rope8a.match(/x/, function() { assertEq(true, false); });
assertEq(tByteSize(rope8a),                                             s(16 + 65536,  24 + 65536));
assertEq(tByteSize(rope8),                                              s(16, 24));


// A char16_t rope. This changes size when flattened.
// "From the Heliconian Muses let us begin to sing"
//   --- Hesiod, Theogony
var fragment16 = "μουσάων Ἑλικωνιάδων ἀρχώμεθ᾽ ἀείδειν";
var rope16 = fragment16;
for (var i = 0; i < 10; i++) // 1024 repetitions
  rope16 = rope16 + rope16;
assertEq(tByteSize(rope16),                                     s(16,  24));
let matches16 = rope16.match(/(Ἑλικωνιάδων ἀρχώμεθ᾽)/);
assertEq(tByteSize(rope16),                                     s(16 + 131072,  24 + 131072));

// Latin-1 and char16_t dependent strings.
assertEq(tByteSize(rope8.substr(1000, 2000)),                   s(16,  24));
assertEq(tByteSize(rope16.substr(1000, 2000)),                  s(16,  24));
assertEq(tByteSize(matches8[0]),                                s(16,  24));
assertEq(tByteSize(matches8[1]),                                s(16,  24));
assertEq(tByteSize(matches16[0]),                               s(16,  24));
assertEq(tByteSize(matches16[1]),                               s(16,  24));

// Test extensible strings.
//
// Appending another copy of the fragment should yield another rope.
//
// Flatting that should turn the original rope into a dependent string, and
// yield a new linear string, of the some size as the original.
rope16a = rope16 + fragment16;
assertEq(tByteSize(rope16a),                                    s(16, 24));
rope16a.match(/x/, function() { assertEq(true, false); });
assertEq(tByteSize(rope16a),                                    s(16 + 131072,  24 + 131072));
assertEq(tByteSize(rope16),                                     s(16, 24));
