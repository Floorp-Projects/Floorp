load(libdir + "asserts.js");

source = `#_\\u200C`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);
