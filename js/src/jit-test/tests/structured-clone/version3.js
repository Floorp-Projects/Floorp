// Created with JS_STRUCTURED_CLONE_VERSION = 3
// var x = {
//     "ab": 1,
//     12: 2,
// };
// print(uneval(serialize(x).clonebuffer));

var clonebuffer = serialize("abc");
clonebuffer.clonebuffer = "\x00\x00\x00\x00\b\x00\xFF\xFF\f\x00\x00\x00\x03\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00@\x02\x00\x00\x00\x04\x00\xFF\xFFa\x00b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF0?\x00\x00\x00\x00\x00\x00\xFF\xFF"
var obj = deserialize(clonebuffer)
assertEq(obj.ab, 1);
assertEq(obj[12], 2);
assertEq(Object.keys(obj).toString(), "12,ab");
