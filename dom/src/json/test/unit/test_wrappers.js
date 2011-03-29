
// returns a list of [string, object] pairs to test encoding
function getTestPairs() {
  var testPairs = [
    ["{}", {}],
    ["[]", []],
    ['{"foo":"bar"}', {"foo":"bar"}],
    ['{"null":null}', {"null":null}],
    ['{"five":5}', {"five":5}],
    ['{"five":5,"six":6}', {"five":5, "six":6}],
    ['{"x":{"y":"z"}}', {"x":{"y":"z"}}],
    ['{"w":{"x":{"y":"z"}}}', {"w":{"x":{"y":"z"}}}],
    ['[1,2,3]', [1,2,3]],
    ['{"w":{"x":{"y":[1,2,3]}}}', {"w":{"x":{"y":[1,2,3]}}}],
    ['{"false":false}', {"false":false}],
    ['{"true":true}', {"true":true}],
    ['{"child has two members":{"this":"one","2":"and this one"}}',
     {"child has two members": {"this":"one", 2:"and this one"}}],
    ['{"x":{"a":"b","c":{"y":"z"},"f":"g"}}',
     {"x":{"a":"b","c":{"y":"z"},"f":"g"}}],
    ['{"x":[1,{"y":"z"},3]}', {"x":[1,{"y":"z"},3]}],
    ['["hmm"]', [new String("hmm")]],
    ['[true]', [new Boolean(true)]],
    ['[42]', [new Number(42)]],
    ['["1978-09-13T12:24:34.023Z"]', [new Date(Date.UTC(1978, 8, 13, 12, 24, 34, 23))]],
    ['[1,null,3]',[1,,3]],
    ['{"mm\\\"mm":"hmm"}',{"mm\"mm":"hmm"}],
    ['{"mm\\\"mm\\\"mm":"hmm"}',{"mm\"mm\"mm":"hmm"}],
    ['{"\\\"":"hmm"}',{'"':"hmm"}],
    ['{"\\\\":"hmm"}',{'\\':"hmm"}],
    ['{"mmm\\\\mmm":"hmm"}',{'mmm\\mmm':"hmm"}],
    ['{"mmm\\\\mmm\\\\mmm":"hmm"}',{'mmm\\mmm\\mmm':"hmm"}],
    ['{"mm\\u000bmm":"hmm"}',{"mm\u000bmm":"hmm"}],
    ['{"mm\\u0000mm":"hmm"}',{"mm\u0000mm":"hmm"}]
  ];

  var x = {"free":"variable"}
  testPairs.push(['{"free":"variable"}', x]);
  testPairs.push(['{"y":{"free":"variable"}}', {"y":x}]);

  // array prop
  var x = {
    a: [1,2,3]
  }
  testPairs.push(['{"a":[1,2,3]}', x])

  var y = {
    foo: function(hmm) { return hmm; }
  }
  testPairs.push(['{"y":{}}',{"y":y}]);

  // test toJSON
  var hmm = {
    toJSON: function() { return {"foo":"bar"}}
  }
  testPairs.push(['{"hmm":{"foo":"bar"}}', {"hmm":hmm}]);
  testPairs.push(['{"foo":"bar"}', hmm]); // on the root

  // toJSON on prototype
  var Y = function() {
    this.not = "there?";
    this.d = "e";
  }
  Y.prototype = {
    not:"there?",
    toJSON: function() { return {"foo":"bar"}}
  };
  var y = new Y();
  testPairs.push(['{"foo":"bar"}', y.toJSON()]);
  testPairs.push(['{"foo":"bar"}', y]);

  // return undefined from toJSON
  var hmm = {
    toJSON: function() { return; }
  }
  testPairs.push(['{}', {"hmm":hmm}]);

  // array with named prop
  var x= new Array();
  x[0] = 1;
  x.foo = "bar";
  testPairs.push(['[1]', x]);

  // prototype
  var X = function() { this.a = "b" }
  X.prototype = {c:"d"}
  var y = new X();
  testPairs.push(['{"a":"b"}', y]);

  return testPairs;
}

function testStringEncode() {
  var pairs = getTestPairs();
  for each(pair in pairs) {
    print(pair)
    var nativeResult = JSON.stringify(pair[1]);
    do_check_eq(pair[0], nativeResult);
  }
}

function decode_strings() {
  // empty object
  var x = JSON.parse("{}");
  do_check_eq(typeof x, "object");

  // empty array
  x = JSON.parse("[]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 0);
  do_check_eq(x.constructor, Array);

  // one element array
  x = JSON.parse("[[]]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 1);
  do_check_eq(x.constructor, Array);
  do_check_eq(x[0].constructor, Array);

  // multiple arrays
  x = JSON.parse("[[],[],[]]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 3);
  do_check_eq(x.constructor, Array);
  do_check_eq(x[0].constructor, Array);
  do_check_eq(x[1].constructor, Array);
  do_check_eq(x[2].constructor, Array);

  // array key/value
  x = JSON.parse('{"foo":[]}');
  do_check_eq(typeof x, "object");
  do_check_eq(typeof x.foo, "object");
  do_check_eq(x.foo.constructor, Array);
  x = JSON.parse('{"foo":[], "bar":[]}');
  do_check_eq(typeof x, "object");
  do_check_eq(typeof x.foo, "object");
  do_check_eq(x.foo.constructor, Array);
  do_check_eq(typeof x.bar, "object");
  do_check_eq(x.bar.constructor, Array);

  // nesting
  x = JSON.parse('{"foo":[{}]}');
  do_check_eq(x.foo.constructor, Array);
  do_check_eq(x.foo.length, 1);
  do_check_eq(typeof x.foo[0], "object");
  x = JSON.parse('{"foo":[{"foo":[{"foo":{}}]}]}');
  do_check_eq(x.foo[0].foo[0].foo.constructor, Object);
  x = JSON.parse('{"foo":[{"foo":[{"foo":[]}]}]}');
  do_check_eq(x.foo[0].foo[0].foo.constructor, Array);

  // strings
  x = JSON.parse('{"foo":"bar"}');
  do_check_eq(x.foo, "bar");
  x = JSON.parse('["foo", "bar", "baz"]');
  do_check_eq(x[0], "foo");
  do_check_eq(x[1], "bar");
  do_check_eq(x[2], "baz");

  // numbers
  x = JSON.parse('{"foo":5.5, "bar":5}');
  do_check_eq(x.foo, 5.5);
  do_check_eq(x.bar, 5);
  
  // keywords
  x = JSON.parse('{"foo": true, "bar":false, "baz":null}');
  do_check_eq(x.foo, true);
  do_check_eq(x.bar, false);
  do_check_eq(x.baz, null);
  
  // short escapes
  x = JSON.parse('{"foo": "\\"", "bar":"\\\\", "baz":"\\b","qux":"\\f", "quux":"\\n", "quuux":"\\r","quuuux":"\\t"}');
  do_check_eq(x.foo, '"');
  do_check_eq(x.bar, '\\');
  do_check_eq(x.baz, '\b');
  do_check_eq(x.qux, '\f');
  do_check_eq(x.quux, "\n");
  do_check_eq(x.quuux, "\r");
  do_check_eq(x.quuuux, "\t");

  // unicode escape
  x = JSON.parse('{"foo":"hmm\\u006dmm"}');
  do_check_eq("hmm\u006dmm", x.foo);

  x = JSON.parse('{"JSON Test Pattern pass3": {"The outermost value": "must be an object or array.","In this test": "It is an object." }}');
}

function run_test() {
  testStringEncode();
  decode_strings();
}
