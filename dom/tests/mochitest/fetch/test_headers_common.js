//
// Utility functions
//

function shouldThrow(func, expected, msg) {
  var err;
  try {
    func();
  } catch (e) {
    err = e;
  } finally {
    ok(err instanceof expected, msg);
  }
}

function recursiveArrayCompare(actual, expected) {
  is(
    Array.isArray(actual),
    Array.isArray(expected),
    "Both should either be arrays, or not"
  );
  if (Array.isArray(actual) && Array.isArray(expected)) {
    var diff = actual.length !== expected.length;

    for (var i = 0, n = actual.length; !diff && i < n; ++i) {
      diff = recursiveArrayCompare(actual[i], expected[i]);
    }

    return diff;
  } else {
    return actual !== expected;
  }
}

function arrayEquals(actual, expected, msg) {
  if (actual === expected) {
    return;
  }

  var diff = recursiveArrayCompare(actual, expected);

  ok(!diff, msg);
  if (diff) {
    is(actual, expected, msg);
  }
}

function checkHas(headers, name, msg) {
  function doCheckHas(n) {
    return headers.has(n);
  }
  return _checkHas(doCheckHas, headers, name, msg);
}

function checkNotHas(headers, name, msg) {
  function doCheckNotHas(n) {
    return !headers.has(n);
  }
  return _checkHas(doCheckNotHas, headers, name, msg);
}

function _checkHas(func, headers, name, msg) {
  ok(func(name), msg);
  ok(func(name.toLowerCase()), msg);
  ok(func(name.toUpperCase()), msg);
}

function checkGet(headers, name, expected, msg) {
  is(headers.get(name), expected, msg);
  is(headers.get(name.toLowerCase()), expected, msg);
  is(headers.get(name.toUpperCase()), expected, msg);
}

//
// Test Cases
//

function TestCoreBehavior(headers, name) {
  var start = headers.get(name);

  headers.append(name, "bar");

  checkHas(headers, name, "Has the header");
  var expected = start ? start.concat(", bar") : "bar";
  checkGet(headers, name, expected, "Retrieve all headers for name");

  headers.append(name, "baz");
  checkHas(headers, name, "Has the header");
  expected = start ? start.concat(", bar, baz") : "bar, baz";
  checkGet(headers, name, expected, "Retrieve all headers for name");

  headers.set(name, "snafu");
  checkHas(headers, name, "Has the header after set");
  checkGet(headers, name, "snafu", "Retrieve all headers after set");

  const value_bam = "boom";
  let testHTTPWhitespace = ["\t", "\n", "\r", " "];
  while (testHTTPWhitespace.length) {
    headers.delete(name);

    let char = testHTTPWhitespace.shift();
    headers.set(name, char);
    checkGet(
      headers,
      name,
      "",
      "Header value " +
        char +
        " should be normalized before checking and throwing"
    );
    headers.delete(name);

    let valueFront = char + value_bam;
    headers.set(name, valueFront);
    checkGet(
      headers,
      name,
      value_bam,
      "Header value " +
        valueFront +
        " should be normalized before checking and throwing"
    );

    headers.delete(name);

    let valueBack = value_bam + char;
    headers.set(name, valueBack);
    checkGet(
      headers,
      name,
      value_bam,
      "Header value " +
        valueBack +
        " should be normalized before checking and throwing"
    );
  }

  headers.delete(name.toUpperCase());
  checkNotHas(headers, name, "Does not have the header after delete");
  checkGet(headers, name, null, "Retrieve all headers after delete");

  // should be ok to delete non-existent name
  headers.delete(name);

  shouldThrow(
    function() {
      headers.append("foo,", "bam");
    },
    TypeError,
    "Append invalid header name should throw TypeError."
  );

  shouldThrow(
    function() {
      headers.append(name, "ba\nm");
    },
    TypeError,
    "Append invalid header value should throw TypeError."
  );

  shouldThrow(
    function() {
      headers.append(name, "ba\rm");
    },
    TypeError,
    "Append invalid header value should throw TypeError."
  );

  ok(!headers.guard, "guard should be undefined in content");
}

function TestEmptyHeaders() {
  is(typeof Headers, "function", "Headers global constructor exists.");
  var headers = new Headers();
  ok(headers, "Constructed empty Headers object");
  TestCoreBehavior(headers, "foo");
}

function TestFilledHeaders() {
  var source = new Headers();
  var filled = new Headers(source);
  ok(filled, "Fill header from empty header");
  TestCoreBehavior(filled, "foo");

  source = new Headers();
  source.append("abc", "123");
  source.append("def", "456");
  source.append("def", "789");

  filled = new Headers(source);
  checkGet(
    filled,
    "abc",
    source.get("abc"),
    "Single value header list matches"
  );
  checkGet(
    filled,
    "def",
    source.get("def"),
    "Multiple value header list matches"
  );
  TestCoreBehavior(filled, "def");

  filled = new Headers({
    zxy: "987",
    xwv: "654",
    uts: "321",
  });
  checkGet(filled, "zxy", "987", "Has first object filled key");
  checkGet(filled, "xwv", "654", "Has second object filled key");
  checkGet(filled, "uts", "321", "Has third object filled key");
  TestCoreBehavior(filled, "xwv");

  filled = new Headers([
    ["zxy", "987"],
    ["xwv", "654"],
    ["xwv", "abc"],
    ["uts", "321"],
  ]);
  checkGet(filled, "zxy", "987", "Has first sequence filled key");
  checkGet(filled, "xwv", "654, abc", "Has second sequence filled key");
  checkGet(filled, "uts", "321", "Has third sequence filled key");
  TestCoreBehavior(filled, "xwv");

  shouldThrow(
    function() {
      filled = new Headers([
        ["zxy", "987", "654"],
        ["uts", "321"],
      ]);
    },
    TypeError,
    "Fill with non-tuple sequence should throw TypeError."
  );

  shouldThrow(
    function() {
      filled = new Headers([["zxy"], ["uts", "321"]]);
    },
    TypeError,
    "Fill with non-tuple sequence should throw TypeError."
  );
}

function iterate(iter) {
  var result = [];
  for (var val = iter.next(); !val.done; ) {
    result.push(val.value);
    val = iter.next();
  }
  return result;
}

function iterateForOf(iter) {
  var result = [];
  for (var value of iter) {
    result.push(value);
  }
  return result;
}

function byteInflate(str) {
  var encoder = new TextEncoder("utf-8");
  var encoded = encoder.encode(str);
  var result = "";
  for (var i = 0; i < encoded.length; ++i) {
    result += String.fromCharCode(encoded[i]);
  }
  return result;
}

function TestHeadersIterator() {
  var ehsanInflated = byteInflate("احسان");
  var headers = new Headers();
  headers.set("foo0", "bar0");
  headers.append("foo", "bar");
  headers.append("foo", ehsanInflated);
  headers.append("Foo2", "bar2");
  headers.set("Foo2", "baz2");
  headers.set("foo3", "bar3");
  headers.delete("foo0");
  headers.delete("foo3");

  var key_iter = headers.keys();
  var value_iter = headers.values();
  var entries_iter = headers.entries();

  arrayEquals(iterate(key_iter), ["foo", "foo2"], "Correct key iterator");
  arrayEquals(
    iterate(value_iter),
    ["bar, " + ehsanInflated, "baz2"],
    "Correct value iterator"
  );
  arrayEquals(
    iterate(entries_iter),
    [
      ["foo", "bar, " + ehsanInflated],
      ["foo2", "baz2"],
    ],
    "Correct entries iterator"
  );

  arrayEquals(
    iterateForOf(headers),
    [
      ["foo", "bar, " + ehsanInflated],
      ["foo2", "baz2"],
    ],
    "Correct entries iterator"
  );
  arrayEquals(
    iterateForOf(new Headers(headers)),
    [
      ["foo", "bar, " + ehsanInflated],
      ["foo2", "baz2"],
    ],
    "Correct entries iterator"
  );
}

function runTest() {
  TestEmptyHeaders();
  TestFilledHeaders();
  TestHeadersIterator();
  return Promise.resolve();
}
