// |jit-test| skip-if: !getBuildConfiguration("moz-memory")
// Run this test only if we're using jemalloc. Other malloc implementations
// exhibit surprising behaviors. For example, 32-bit Fedora builds have
// non-deterministic allocation sizes.

// Check JS::ubi::Node::size results for strings.

// We actually hard-code specific sizes into this test, even though they're
// implementation details, because in practice there are only two architecture
// variants to consider (32-bit and 64-bit), and if these sizes change, that's
// something SpiderMonkey hackers really want to know; they're supposed to be
// stable.

gczeal(0); // Need to control when tenuring happens

// Hack to skip this test if strings are not allocated in the nursery.
{
  const sample_nursery = "x" + "abc".substr(1);
  let nursery_enabled = true;
  const before = byteSize(sample_nursery);
  gc();
  const after = byteSize(sample_nursery);
  if (before == after)
    nursery_enabled = false;
  if (!nursery_enabled) {
    printErr("nursery strings appear to be disabled");
    quit(0);
  }
}

// Ion eager runs much of this code in Ion, and Ion nursery-allocates more
// aggressively than other modes.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 100)
    setJitCompilerOption("ion.warmup.trigger", 100);

if (getBuildConfiguration("pointer-byte-size") == 4)
  var s = (s32, s64) => s32
else
  var s = (s32, s64) => s64

// Convert an input string, which is probably an atom because it's a literal in
// the source text, to a nursery-allocated string with the same contents. Note
// that by going through the flatting process here, this also ensures that the
// char data is always malloced and expanded by doubling, which would not be
// the case for nursery-allocated chars (as would be produced by newString()).
function copyString(str) {
  if (str.length == 0)
    return str; // Nothing we can do here
  return ensureLinearString(str.substr(0, 1) + str.substr(1));
}

// Return the nursery byte size of |str|.
function nByteSize(str) {
  // Strings that appear in the source will always be atomized and therefore
  // will never be in the nursery.
  return byteSize(copyString(str));
}

// Return the tenured byte size of |str|.
function tByteSize(str) {
  // Strings that appear in the source will always be atomized and therefore
  // will never be in the nursery. But we'll make them get tenured instead of
  // using the atom.
  str = copyString(str);
  minorgc();
  return byteSize(str);
}

// There are four representations of linear strings, with the following
// capacities:
//
//                      32-bit                  64-bit                test
// representation       Latin-1   char16_t      Latin-1   char16_t    label
// ========================================================================
// JSExternalString            - limited by MaxStringLength -         E
// JSThinInlineString   8         4             16        8           T
// JSFatInlineString    24        12            24        12          F
// JSExtensibleString          - limited by MaxStringLength -         X

// Notes:
//  - labels are suffixed with A for atoms and N for non-atoms
//  - atoms are 8 bytes larger than non-atoms, to store the atom's hash code.
//  - Nursery-allocated strings require a header that stores the zone.

// Expected sizes based on type of string
const m32 = (getBuildConfiguration("pointer-byte-size") == 4);
const TA = m32 ? 24 : 32; // ThinInlineString atom, includes a hash value
const TN = m32 ? 16 : 24; // ThinInlineString
const FN = m32 ? 32 : 32; // FatInlineString
const XN = m32 ? 16 : 24; // ExtensibleString, has additional storage buffer
const RN = m32 ? 16 : 24; // Rope
const DN = m32 ? 16 : 24; // DependentString
const EN = m32 ? 16 : 24; // ExternalString

// A function that pads out a tenured size to the nursery size. We store a zone
// pointer in the nursery just before the string (4 bytes on 32-bit, 8 bytes on
// 64-bit), and the string struct itself must be 8-byte aligned (resulting in
// +4 bytes on 32-bit, +0 bytes on 64-bit). The end result? Nursery strings are
// 8 bytes larger.
const Nursery = m32 ? s => s + 4 + 4 : s => s + 8 + 0;

// Latin-1
assertEq(tByteSize(""),                                               s(TA, TA));
assertEq(tByteSize("1"),                                              s(TA, TA));
assertEq(tByteSize("1234567"),                                        s(TN, TN));
assertEq(tByteSize("12345678"),                                       s(TN, TN));
assertEq(tByteSize("123456789"),                                      s(FN, TN));
assertEq(tByteSize("123456789.12345"),                                s(FN, TN));
assertEq(tByteSize("123456789.123456"),                               s(FN, TN));
assertEq(tByteSize("123456789.1234567"),                              s(FN, FN));
assertEq(tByteSize("123456789.123456789.123"),                        s(FN, FN));
assertEq(tByteSize("123456789.123456789.1234"),                       s(FN, FN));
assertEq(tByteSize("123456789.123456789.12345"),                      s(XN+32, XN+32));
assertEq(tByteSize("123456789.123456789.123456789.1"),                s(XN+32, XN+32));
assertEq(tByteSize("123456789.123456789.123456789.12"),               s(XN+32, XN+32));
assertEq(tByteSize("123456789.123456789.123456789.123"),              s(XN+64, XN+64));

assertEq(nByteSize(""),                                               s(TA, TA));
assertEq(nByteSize("1"),                                              s(TA, TA));
assertEq(nByteSize("1234567"),                                        s(Nursery(TN), Nursery(TN)));
assertEq(nByteSize("12345678"),                                       s(Nursery(TN), Nursery(TN)));
assertEq(nByteSize("123456789"),                                      s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("123456789.12345"),                                s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("123456789.123456"),                               s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("123456789.1234567"),                              s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("123456789.123456789.123"),                        s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("123456789.123456789.1234"),                       s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("123456789.123456789.12345"),                      s(Nursery(XN)+32,Nursery(XN)+32));
assertEq(nByteSize("123456789.123456789.123456789.1"),                s(Nursery(XN)+32,Nursery(XN)+32));
assertEq(nByteSize("123456789.123456789.123456789.12"),               s(Nursery(XN)+32,Nursery(XN)+32));
assertEq(nByteSize("123456789.123456789.123456789.123"),              s(Nursery(XN)+64,Nursery(XN)+64));

// Inline char16_t atoms.
// "Impassionate gods have never seen the red that is the Tatsuta River."
//   - Ariwara no Narihira
assertEq(tByteSize("千"),						s(TA, TA));
assertEq(tByteSize("千早"),						s(TN, TN));
assertEq(tByteSize("千早ぶ"),						s(TN, TN));
assertEq(tByteSize("千早ぶる"),						s(TN, TN));
assertEq(tByteSize("千早ぶる神"),						s(FN, TN));
assertEq(tByteSize("千早ぶる神代"),					s(FN, TN));
assertEq(tByteSize("千早ぶる神代も"),					s(FN, TN));
assertEq(tByteSize("千早ぶる神代もき"),					s(FN, TN));
assertEq(tByteSize("千早ぶる神代もきか"),					s(FN, FN));
assertEq(tByteSize("千早ぶる神代もきかず龍"),				s(FN, FN));
assertEq(tByteSize("千早ぶる神代もきかず龍田"),				s(FN, FN));
assertEq(tByteSize("千早ぶる神代もきかず龍田川"),				s(XN+32, XN+32));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 か"),				s(XN+32, XN+32));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 から"),			s(XN+32, XN+32));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からく"),		s(XN+64, XN+64));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水く"),		s(XN+64, XN+64));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くく"),		s(XN+64, XN+64));
assertEq(tByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くくるとは"),	s(XN+64, XN+64));

assertEq(nByteSize("千"),						s(TA, TA));
assertEq(nByteSize("千早"),						s(Nursery(TN), Nursery(TN)));
assertEq(nByteSize("千早ぶ"),						s(Nursery(TN), Nursery(TN)));
assertEq(nByteSize("千早ぶる"),						s(Nursery(TN), Nursery(TN)));
assertEq(nByteSize("千早ぶる神"),						s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("千早ぶる神代"),					s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("千早ぶる神代も"),					s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("千早ぶる神代もき"),					s(Nursery(FN), Nursery(TN)));
assertEq(nByteSize("千早ぶる神代もきか"),					s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("千早ぶる神代もきかず龍"),				s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("千早ぶる神代もきかず龍田"),				s(Nursery(FN), Nursery(FN)));
assertEq(nByteSize("千早ぶる神代もきかず龍田川"),				s(Nursery(XN)+32, Nursery(XN)+32));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 か"),				s(Nursery(XN)+32, Nursery(XN)+32));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 から"),			s(Nursery(XN)+32, Nursery(XN)+32));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 からく"),		s(Nursery(XN)+64, Nursery(XN)+64));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水く"),		s(Nursery(XN)+64, Nursery(XN)+64));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くく"),		s(Nursery(XN)+64, Nursery(XN)+64));
assertEq(nByteSize("千早ぶる神代もきかず龍田川 からくれなゐに水くくるとは"),	s(Nursery(XN)+64, Nursery(XN)+64));

// A Latin-1 rope. This changes size when flattened.
// "In a village of La Mancha, the name of which I have no desire to call to mind"
//   - Miguel de Cervantes, Don Quixote
var fragment8 = "En un lugar de la Mancha, de cuyo nombre no quiero acordarme"; // 60 characters
var rope8 = fragment8;
for (var i = 0; i < 10; i++) // 1024 repetitions
  rope8 = rope8 + rope8;

assertEq(byteSize(rope8),                                               s(Nursery(RN), Nursery(RN)));
minorgc();
assertEq(byteSize(rope8),                                               s(RN, RN));
var matches8 = rope8.match(/(de cuyo nombre no quiero acordarme)/);
assertEq(byteSize(rope8),                                               s(XN + 65536, XN + 65536));

// Test extensible strings.
//
// Appending another copy of the fragment should yield another rope.
//
// Flatting that should turn the original rope into a dependent string, and
// yield a new linear string, of the same size as the original.
rope8a = rope8 + fragment8;
assertEq(byteSize(rope8a),                                              s(Nursery(RN), Nursery(RN)));
rope8a.match(/x/, function() { assertEq(true, false); });
assertEq(byteSize(rope8a),                                              s(Nursery(XN) + 65536, Nursery(XN) + 65536));
assertEq(byteSize(rope8),                                               s(RN, RN));


// A char16_t rope. This changes size when flattened.
// "From the Heliconian Muses let us begin to sing"
//   --- Hesiod, Theogony
var fragment16 = "μουσάων Ἑλικωνιάδων ἀρχώμεθ᾽ ἀείδειν";
var rope16 = fragment16;
for (var i = 0; i < 10; i++) // 1024 repetitions
  rope16 = rope16 + rope16;
assertEq(byteSize(rope16),                                              s(Nursery(RN), Nursery(RN)));
let matches16 = rope16.match(/(Ἑλικωνιάδων ἀρχώμεθ᾽)/);
assertEq(byteSize(rope16),                                              s(Nursery(RN) + 131072, Nursery(RN) + 131072));

// Latin-1 and char16_t dependent strings.
assertEq(byteSize(rope8.substr(1000, 2000)),                            s(Nursery(DN), Nursery(DN)));
assertEq(byteSize(rope16.substr(1000, 2000)),                           s(Nursery(DN), Nursery(DN)));
assertEq(byteSize(matches8[0]),                                         s(Nursery(DN), Nursery(DN)));
assertEq(byteSize(matches8[1]),                                         s(Nursery(DN), Nursery(DN)));
assertEq(byteSize(matches16[0]),                                        s(Nursery(DN), Nursery(DN)));
assertEq(byteSize(matches16[1]),                                        s(Nursery(DN), Nursery(DN)));

// Test extensible strings.
//
// Appending another copy of the fragment should yield another rope.
//
// Flatting that should turn the original rope into a dependent string, and
// yield a new linear string, of the some size as the original.
rope16a = rope16 + fragment16;
assertEq(byteSize(rope16a),                                             s(Nursery(RN), Nursery(RN)));
rope16a.match(/x/, function() { assertEq(true, false); });
assertEq(byteSize(rope16a),                                             s(Nursery(XN) + 131072, Nursery(XN) + 131072));
assertEq(byteSize(rope16),                                              s(Nursery(XN), Nursery(XN)));

// Test external strings.
//
// We only support char16_t external strings and external strings are never
// allocated in the nursery. If this ever changes, please add tests for the new
// cases. Also note that on Windows mozmalloc's smallest allocation size is
// two words compared to one word on other platforms.
if (getBuildConfiguration("windows")) {
  assertEq(byteSize(newString("", {external: true})),                        s(EN+8, EN+16));
  assertEq(byteSize(newString("1", {external: true})),                       s(EN+8, EN+16));
  assertEq(byteSize(newString("12", {external: true})),                      s(EN+8, EN+16));
  assertEq(byteSize(newString("123", {external: true})),                     s(EN+8, EN+16));
  assertEq(byteSize(newString("1234", {external: true})),                    s(EN+8, EN+16));
} else {
  assertEq(byteSize(newString("", {external: true})),                        s(EN+4, EN+8));
  assertEq(byteSize(newString("1", {external: true})),                       s(EN+4, EN+8));
  assertEq(byteSize(newString("12", {external: true})),                      s(EN+4, EN+8));
  assertEq(byteSize(newString("123", {external: true})),                     s(EN+8, EN+8));
  assertEq(byteSize(newString("1234", {external: true})),                    s(EN+8, EN+8));
}
assertEq(byteSize(newString("12345", {external: true})),                     s(EN+16, EN+16));
assertEq(byteSize(newString("123456789.123456789.1234", {external: true})),  s(EN+48, EN+48));
assertEq(byteSize(newString("123456789.123456789.12345", {external: true})), s(EN+64, EN+64));

// Nursery-allocated chars.
//
// byteSize will not include the space used by the nursery for the chars.
assertEq(byteSize(newString("123456789.123456789.12345")), s(Nursery(XN)+0,Nursery(XN)+0));
assertEq(byteSize(newString("123456789.123456789.123456789.123")), s(Nursery(XN)+0,Nursery(XN)+0));
