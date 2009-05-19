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
}
