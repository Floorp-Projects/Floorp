// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Josh Matthews

var p = Proxy.create({ fix: function() { return {foo: {value: 2} }; } });
Object.preventExtensions(p);
reportCompare(p.foo, 2, "property exists");
p.foo = 3;
reportCompare(p.foo, 2, "proxy is frozen");