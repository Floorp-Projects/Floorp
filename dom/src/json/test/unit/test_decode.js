function decode_strings() {
  // empty object
  var x = nativeJSON.decode("{}");
  do_check_eq(typeof x, "object");

  // empty array
  x = nativeJSON.decode("[]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 0);
  do_check_eq(x.constructor, Array);

  // one element array
  x = nativeJSON.decode("[[]]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 1);
  do_check_eq(x.constructor, Array);
  do_check_eq(x[0].constructor, Array);

  // multiple arrays
  x = nativeJSON.decode("[[],[],[]]");
  do_check_eq(typeof x, "object");
  do_check_eq(x.length, 3);
  do_check_eq(x.constructor, Array);
  do_check_eq(x[0].constructor, Array);
  do_check_eq(x[1].constructor, Array);
  do_check_eq(x[2].constructor, Array);

  // array key/value
  x = nativeJSON.decode('{"foo":[]}');
  do_check_eq(typeof x, "object");
  do_check_eq(typeof x.foo, "object");
  do_check_eq(x.foo.constructor, Array);
  x = nativeJSON.decode('{"foo":[], "bar":[]}');
  do_check_eq(typeof x, "object");
  do_check_eq(typeof x.foo, "object");
  do_check_eq(x.foo.constructor, Array);
  do_check_eq(typeof x.bar, "object");
  do_check_eq(x.bar.constructor, Array);

  // nesting
  x = nativeJSON.decode('{"foo":[{}]}');
  do_check_eq(x.foo.constructor, Array);
  do_check_eq(x.foo.length, 1);
  do_check_eq(typeof x.foo[0], "object");
  x = nativeJSON.decode('{"foo":[{"foo":[{"foo":{}}]}]}');
  do_check_eq(x.foo[0].foo[0].foo.constructor, Object);
  x = nativeJSON.decode('{"foo":[{"foo":[{"foo":[]}]}]}');
  do_check_eq(x.foo[0].foo[0].foo.constructor, Array);

  // strings
  x = nativeJSON.decode('{"foo":"bar"}');
  do_check_eq(x.foo, "bar");
  x = nativeJSON.decode('["foo", "bar", "baz"]');
  do_check_eq(x[0], "foo");
  do_check_eq(x[1], "bar");
  do_check_eq(x[2], "baz");

  // numbers
  x = nativeJSON.decode('{"foo":5.5, "bar":5}');
  do_check_eq(x.foo, 5.5);
  do_check_eq(x.bar, 5);
  
  // keywords
  x = nativeJSON.decode('{"foo": true, "bar":false, "baz":null}');
  do_check_eq(x.foo, true);
  do_check_eq(x.bar, false);
  do_check_eq(x.baz, null);
  
  // short escapes
  x = nativeJSON.decode('{"foo": "\\"", "bar":"\\\\", "baz":"\\b","qux":"\\f", "quux":"\\n", "quuux":"\\r","quuuux":"\\t"}');
  do_check_eq(x.foo, '"');
  do_check_eq(x.bar, '\\');
  do_check_eq(x.baz, '\b');
  do_check_eq(x.qux, '\f');
  do_check_eq(x.quux, "\n");
  do_check_eq(x.quuux, "\r");
  do_check_eq(x.quuuux, "\t");

  // unicode escape
  x = nativeJSON.decode('{"foo":"hmm\\u006dmm"}');
  do_check_eq("hmm\u006dmm", x.foo);

  x = nativeJSON.decode('{"JSON Test Pattern pass3": {"The outermost value": "must be an object or array.","In this test": "It is an object." }}');
}

function test_files() {
  function read_file(path) {
    try {
      var f = do_get_file(path);
      var istream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
      istream.init(f, -1, -1, false);
      var x = nativeJSON.decodeFromStream(istream, istream.available());
    } finally {
      istream.close();
    }
    return x;
  }

  var x = read_file("pass3.json");
  do_check_eq(x["JSON Test Pattern pass3"]["The outermost value"], "must be an object or array.");
  do_check_eq(x["JSON Test Pattern pass3"]["In this test"], "It is an object.");

  x = read_file("pass1.json");
  do_check_eq(x[0], "JSON Test Pattern pass1");
  do_check_eq(x[1]["object with 1 member"][0], "array with 1 element");
  do_check_eq(x[2].constructor, Object);
  do_check_eq(x[3].constructor, Array);
  do_check_eq(x[4], -42);
  do_check_eq(x[5], true);
  do_check_eq(x[6], false);
  do_check_eq(x[7], null);
  do_check_eq(x[8].constructor, Object);
  do_check_eq(x[8]["integer"], 1234567890);
  do_check_eq(x[8]["real"], -9876.543210);
  do_check_eq(x[8]["e"], 0.123456789e-12);
  do_check_eq(x[8]["E"], 1.234567890E+34);
  do_check_eq(x[8][""], 23456789012E66);
  do_check_eq(x[8]["zero"], 0);
  do_check_eq(x[8]["one"], 1);
  do_check_eq(x[8]["space"], " ");
  do_check_eq(x[8]["quote"], "\"");
  do_check_eq(x[8]["backslash"], "\\");
  do_check_eq(x[8]["controls"], "\b\f\n\r\t");
  do_check_eq(x[8]["slash"], "/ & /");
  do_check_eq(x[8]["alpha"], "abcdefghijklmnopqrstuvwyz");
  do_check_eq(x[8]["ALPHA"], "ABCDEFGHIJKLMNOPQRSTUVWYZ");
  do_check_eq(x[8]["digit"], "0123456789");
  do_check_eq(x[8]["0123456789"], "digit");
  do_check_eq(x[8]["special"], "`1~!@#$%^&*()_+-={':[,]}|;.</>?");
  do_check_eq(x[8]["hex"], "\u0123\u4567\u89AB\uCDEF\uabcd\uef4A");
  do_check_eq(x[8]["true"], true);
  do_check_eq(x[8]["false"], false);
  do_check_eq(x[8]["null"], null);
  do_check_eq(x[8]["array"].length, 0);
  do_check_eq(x[8]["object"].constructor, Object);
  do_check_eq(x[8]["address"], "50 St. James Street");
  do_check_eq(x[8]["url"], "http://www.JSON.org/");
  do_check_eq(x[8]["comment"], "// /* <!-- --");
  do_check_eq(x[8]["# -- --> */"], " ");
  do_check_eq(x[8][" s p a c e d "].length, 7);
  do_check_eq(x[8]["compact"].length, 7);
  do_check_eq(x[8]["jsontext"], "{\"object with 1 member\":[\"array with 1 element\"]}");
  do_check_eq(x[8]["quotes"], "&#34; \u0022 %22 0x22 034 &#x22;");
  do_check_eq(x[8]["\/\\\"\uCAFE\uBABE\uAB98\uFCDE\ubcda\uef4A\b\f\n\r\t`1~!@#$%^&*()_+-=[]{}|;:',./<>?"], "A key can be any string");
  do_check_eq(x[9], 0.5);
  do_check_eq(x[10], 98.6);
  do_check_eq(x[11], 99.44);
  do_check_eq(x[12], 1066);
  do_check_eq(x[13], 1e1);
  do_check_eq(x[14], 0.1e1);
  do_check_eq(x[15], 1e-1);
  do_check_eq(x[16], 1e00);
  do_check_eq(x[17], 2e+00);
  do_check_eq(x[18], 2e-00);
  do_check_eq(x[19], "rosebud");
  
  // test invalid input
  //
  // We allow some minor JSON infractions, like trailing commas
  // Those are special-cased below, leaving the original sequence
  // of failure's from Crockford intact.
  //
  // Section 4 of RFC 4627 allows this tolerance.
  //
  for (var i = 1; i <= 34; ++i) {
    var path = "fail" + i + ".json";
    try {
      dump(path +"\n");
      x = read_file(path);
      if (i == 13) {
        // {"Numbers cannot have leading zeroes": 013}
        do_check_eq(x["Numbers cannot have leading zeroes"], 13);
      } else if (i == 18) {
        // [[[[[[[[[[[[[[[[[[[["Too deep"]]]]]]]]]]]]]]]]]]]]
        var t = x[0][0][0][0][0][0][0][0][0][0][0][0][0][0][0][0][0][0][0][0];
        do_check_eq(t, "Too deep");
      } else {

        do_throw("UNREACHED");

      }

    } catch (ex) {
      // expected from parsing invalid JSON
      if (i == 13 || i == 18) {
        do_throw("Unexpected pass in " + path);
      }
    }
  }
  
}

function run_test() {
  decode_strings();
  test_files();
}
