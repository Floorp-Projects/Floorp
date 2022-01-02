
try {

function f() {}
f("".match(/(?:(?=g)).{2147483648,}/ + (/[]/)), null);

} catch (e) {} // Yarr throws on the above regexp

var re = new RegExp("a[\x01-\\xDE]+M", "gi");
re.exec("");

var re = new RegExp("a[\x01-\\u00b8]+?c", "gi");
re.exec("");

var re = new RegExp("a[\x01-\\u00f8]?c", "gi");
re.exec("");
