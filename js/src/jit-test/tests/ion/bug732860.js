function trySetProperty(o, p, v, strict) {
  function strictSetProperty()  {
    "use strict";
    o[p] = v;
  }
  try  {
      strictSetProperty();
  }  catch (e)  {
    return "throw";
  }
}

//var objs = [[0], [1]];
var objs = [{a: 0}, {a: 1}];

for (var i = 0, sz = objs.length; i < sz; i++) {
  var o = objs[i];
  var o2 = Object.preventExtensions(o);
  print(i +'  ' + o);
  assertEq(trySetProperty(o, "baz", 17, true), "throw", "object " + i);
}
