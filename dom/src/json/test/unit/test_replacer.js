/**
 * These return* functions are used by the
 * replacer tests taken from bug 512447
 */
function returnObjectFor1(k, v) {
    if (k == "1")
        return {};
    return v;
}
function returnArrayFor1(k, v) {
    if (k == "1")
        return [];
    return v;
}
function returnNullFor1(k, v) {
    if (k == "1")
        return null;
    return v;
}
function returnStringForUndefined(k, v) {
    if (v === undefined)
        return "undefined value";
    return v;
}
function returnCycleObjectFor1(k, v) {
    if (k == "1")
        return object;
    return v;
}
function returnCycleArrayFor1(k, v) {
    if (k == "1")
        return array;
    return v;
}

function run_test() {
  var x = JSON.stringify({key:2},function(k,v){return k?undefined:v;})
  do_check_eq("{}", x);
  
  var x = JSON.stringify(["hmm", "hmm"],function(k,v){return k!==""?undefined:v;})
  do_check_eq("[null,null]", x);
  
  var foo = ["hmm"];
  function censor(k, v) {
    if (v !== foo)
      return "XXX";
    return v;
  }
  var x = JSON.stringify(foo, censor);
  do_check_eq(x, '["XXX"]');

  foo = ["bar", ["baz"], "qux"];
  var x = JSON.stringify(foo, censor);
  do_check_eq(x, '["XXX","XXX","XXX"]');

  function censor2(k, v) {
    if (typeof(v) == "string")
      return "XXX";
    return v;
  }

  foo = ["bar", ["baz"], "qux"];
  var x = JSON.stringify(foo, censor2);
  do_check_eq(x, '["XXX",["XXX"],"XXX"]');

  foo = {bar: 42, qux: 42, quux: 42};
  var x = JSON.stringify(foo, ["bar"]);
  do_check_eq(x, '{"bar":42}');

  foo = {bar: {bar: 42, schmoo:[]}, qux: 42, quux: 42};
  var x = JSON.stringify(foo, ["bar", "schmoo"]);
  do_check_eq(x, '{"bar":{"bar":42,"schmoo":[]}}');

  var x = JSON.stringify(foo, null, "");
  do_check_eq(x, '{"bar":{"bar":42,"schmoo":[]},"qux":42,"quux":42}');

  var x = JSON.stringify(foo, null, "  ");
  do_check_eq(x, '{\n  "bar":{\n    "bar":42,\n    "schmoo":[]\n  },\n  "qux":42,\n  "quux":42\n}');

  foo = {bar:{bar:{}}}
  var x = JSON.stringify(foo, null, "  ");
  do_check_eq(x, '{\n  "bar":{\n    "bar":{}\n  }\n}');
  
  var x = JSON.stringify({x:1,arr:[1]}, function (k,v) { return typeof v === 'number' ? 3 : v; });
  do_check_eq(x, '{"x":3,"arr":[3]}');

  foo = ['e'];
  var x = JSON.stringify(foo, null, '\t');
  do_check_eq(x, '[\n\t"e"\n]');

  foo = {0:0, 1:1, 2:2, 3:undefined};
  var x = JSON.stringify(foo, returnObjectFor1);
  do_check_eq(x, '{"0":0,"1":{},"2":2}');

  var x = JSON.stringify(foo, returnArrayFor1);
  do_check_eq(x, '{"0":0,"1":[],"2":2}');

  var x = JSON.stringify(foo, returnNullFor1);
  do_check_eq(x, '{"0":0,"1":null,"2":2}');

  var x = JSON.stringify(foo, returnStringForUndefined);
  do_check_eq(x, '{"0":0,"1":1,"2":2,"3":"undefined value"}');

  var thrown = false;
  try {
    var x = JSON.stringify(foo, returnCycleObjectFor1);
  } catch (e) {
    thrown = true;
  }
  do_check_eq(thrown, true);

  var thrown = false;
  try {
    var x = JSON.stringify(foo, returnCycleArrayFor1);
  } catch (e) {
    thrown = true;
  }
  do_check_eq(thrown, true);

  var thrown = false;
  foo = [0, 1, 2, undefined];
  try {
    var x = JSON.stringify(foo, returnCycleObjectFor1);
  } catch (e) {
    thrown = true;
  }
  do_check_eq(thrown, true);
}

