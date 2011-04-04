
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
    [null, function test(){}],
    [null, dump],
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
  //testPairs.push(['[1]', x]);

  // prototype
  var X = function() { this.foo = "b" }
  X.prototype = {c:"d"}
  var y = new X();
  testPairs.push(['{"foo":"b"}', y]);

  // useless roots will be dropped
  testPairs.push([null, null]);
  testPairs.push([null, ""]);
  testPairs.push([null, undefined]);
  testPairs.push([null, 5]);

  return testPairs;
}

function testStringEncode() {
  // test empty arg
  do_check_eq(null, nativeJSON.encode());

  var pairs = getTestPairs();
  for each(pair in pairs) {
    var nativeResult = nativeJSON.encode(pair[1]);
    do_check_eq(pair[0], nativeResult);
  }
}

function testOutputStreams() {
  function writeToFile(obj, charset, writeBOM) {
    var jsonFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    jsonFile.initWithFile(outputDir);
    jsonFile.append("test.json");
    jsonFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
    var stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    try {
      stream.init(jsonFile, 0x04 | 0x08 | 0x20, 0600, 0); // write, create, truncate
      nativeJSON.encodeToStream(stream, charset, writeBOM, obj);
    } finally {
      stream.close();
    }
    return jsonFile;
  }

  var pairs = getTestPairs();
  for each(pair in pairs) {
    if (pair[1] && (typeof pair[1] == "object")) {
      var utf8File = writeToFile(pair[1], "UTF-8", false);
      var utf16LEFile = writeToFile(pair[1], "UTF-16LE", false);
      var utf16BEFile = writeToFile(pair[1], "UTF-16BE", false);

      // all ascii with no BOMs, so this will work
      do_check_eq(utf16LEFile.fileSize / 2, utf8File.fileSize);
      do_check_eq(utf16LEFile.fileSize, utf16BEFile.fileSize);
    }
  }

  // check BOMs
  // the clone() calls are there to work around -- bug 410005
  var f = writeToFile({},"UTF-8", true).clone();
  do_check_eq(f.fileSize, 5);
  var f = writeToFile({},"UTF-16LE", true).clone();
  do_check_eq(f.fileSize, 6);
  var f = writeToFile({},"UTF-16BE", true).clone();
  do_check_eq(f.fileSize, 6);
  
  outputDir.remove(true);
}

function throwingToJSON() {
  var a = {
    "b": 1,
    "c": 2,
    toJSON: function() { throw("uh oh"); }
  }
  try {
    var y = nativeJSON.encode(a);
  } catch (ex) {}
}

function run_test() {
  testStringEncode();
  throwingToJSON();
  
  testOutputStreams();
  
}
