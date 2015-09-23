var Ci = Components.interfaces;
var Cc = Components.classes;

var nativeJSON = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var workingDir = dirSvc.get("TmpD", Ci.nsIFile);

var outputName = "json-test-output";
var outputDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
outputDir.initWithFile(workingDir);
outputDir.append(outputName);

if (!outputDir.exists()) {
  outputDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
} else if (!outputDir.isDirectory()) {
  do_throw(outputName + " is not a directory?")
}

function testStringEncode()
{
  var obj1 = {a:1};
  var obj2 = {foo:"bar"};
  do_check_eq(nativeJSON.encode(obj1), '{"a":1}');
  do_check_eq(nativeJSON.encode(obj2), '{"foo":"bar"}');

  do_check_eq(nativeJSON.encode(), null);

  // useless roots are dropped
  do_check_eq(nativeJSON.encode(null), null);
  do_check_eq(nativeJSON.encode(""), null);
  do_check_eq(nativeJSON.encode(undefined), null);
  do_check_eq(nativeJSON.encode(5), null);
  do_check_eq(nativeJSON.encode(function(){}), null);
  do_check_eq(nativeJSON.encode(dump), null);

  // All other testing should occur in js/src/tests/ecma_5/JSON/ using
  // the otherwise-exactly-identical JSON.stringify.
}

function testToJSON() {
  var obj1 = {a:1};
  var obj2 = {foo:"bar"};
  do_check_eq(nativeJSON.encode({toJSON: () => obj1}), '{"a":1}');
  do_check_eq(nativeJSON.encode({toJSON: () => obj2}), '{"foo":"bar"}');
  
  do_check_eq(nativeJSON.encode({toJSON: () => null}), null);
  do_check_eq(nativeJSON.encode({toJSON: () => ""}), null);
  do_check_eq(nativeJSON.encode({toJSON: () => undefined }), null);
  do_check_eq(nativeJSON.encode({toJSON: () => 5}), null);
  do_check_eq(nativeJSON.encode({toJSON: () => function(){}}), null);
  do_check_eq(nativeJSON.encode({toJSON: () => dump}), null);
}

function testThrowingToJSON() {
  var obj1 = {
    "b": 1,
    "c": 2,
    toJSON: function() { throw("uh oh"); }
  };
  try {
    var y = nativeJSON.encode(obj1);
    throw "didn't throw";
  } catch (ex) {
    do_check_eq(ex, "uh oh");
  }

  var obj2 = {
    "b": 1,
    "c": 2,
    get toJSON() { throw("crash and burn"); }
  };
  try {
    var y = nativeJSON.encode(obj2);
    throw "didn't throw";
  } catch (ex) {
    do_check_eq(ex, "crash and burn");
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

  var pairs = [
    ["{}", {}],
    ['{"foo":"bar"}', {"foo":"bar"}],
    ['{"null":null}', {"null":null}],
    ['{"five":5}', {"five":5}],
    ['{"true":true}', {"true":true}],
    ['{"x":{"y":"z"}}', {"x":{"y":"z"}}],
    ['{"w":{"x":{"y":"z"}}}', {"w":{"x":{"y":"z"}}}],
    ["[]", []],
    ['[1,2,3]', [1,2,3]],
    ['[1,null,3]',[1,,3]],
  ];
  for (var i = 0; i < pairs.length; i++)
  {
    var pair = pairs[i];
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

function run_test()
{
  testStringEncode();
  testToJSON();
  testThrowingToJSON();
  
  testOutputStreams();
  
}
