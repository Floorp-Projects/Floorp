load(libdir + "asserts.js");
assertThrowsInstanceOf(function () { eval("({p:"); }, SyntaxError);  // don't crash
