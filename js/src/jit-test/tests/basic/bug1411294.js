oomTest(function() {
  eval(`var clonebuffer = serialize("abc");
  clonebuffer.clonebuffer = "\
\\x00\\x00\\x00\\x00\\b\\x00\\xFF\\xFF\\f\
\\x00\\x00\\x00\\x03\\x00\\xFF\\xFF\\x00\\x00\\x00\\x00\\x00\\x00\\x00\
\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\xF0?\\x00\\x00\\x00\\\x00\\x00\
\\x00\\xFF\\xFF"
  var obj = deserialize(clonebuffer)
  assertEq(new ({ get }).keys(obj).toString(), "12,ab");
`);
});
