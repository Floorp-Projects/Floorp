assertEq(JSON.stringify({foo: 123}),
         '{"foo":123}');
assertEq(JSON.stringify({foo: 123, bar: function () {}}),
         '{"foo":123}');
assertEq(JSON.stringify({foo: 123, bar: function () {}, baz: 123}),
         '{"foo":123,"baz":123}');

assertEq(JSON.stringify([123]),
         '[123]');
assertEq(JSON.stringify([123, function () {}]),
         '[123,null]');
assertEq(JSON.stringify([123, function () {}, 456]),
         '[123,null,456]');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
