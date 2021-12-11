// Binary: cache/js-dbg-64-7054f0e3e70e-linux
// Flags:
//

re = new RegExp("([^b]*)+((..)|(\\3))+?Sc*a!(a|ab)(c|bcd)(<*)", "i");
var str = "aNULLxabcd";
str.replace(re, function(s) { return s; });
