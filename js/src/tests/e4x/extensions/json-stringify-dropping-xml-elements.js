// |reftest| pref(javascript.options.xml.content,true)
// Ported from dom/src/json/test/unit/test_dropping_elements_in_stringify.js

assertEq(JSON.stringify({foo: 123, bar: <x><y></y></x>, baz: 123}),
         '{"foo":123,"baz":123}');

assertEq(JSON.stringify([123, <x><y></y></x>, 456]),
         '[123,null,456]');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
