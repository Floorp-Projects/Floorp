load(libdir + "asserts.js");

m={}
Object.defineProperty(m, 'p', {value: 3});
assertThrowsInstanceOf(function() {"use strict"; delete m.p;}, TypeError);

x = new Proxy(m, {})
assertEq(x.p, 3);
assertEq((function fun() {"use strict"; return delete x.p; })(), false);
