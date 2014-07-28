//
// Utility functions
//

function shouldThrow(func, expected, msg) {
  var err;
  try {
    func();
  } catch(e) {
    err = e;
  } finally {
    ok(err instanceof expected, msg);
  }
}

function arrayEquals(actual, expected, msg) {
  if (actual === expected) {
    return;
  }

  var diff = actual.length !== expected.length;

  for (var i = 0, n = actual.length; !diff && i < n; ++i) {
    diff = actual[i] !== expected[i];
  }

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
  ok(func(name.toLowerCase()), msg)
  ok(func(name.toUpperCase()), msg)
}

function checkGet(headers, name, expected, msg) {
  is(headers.get(name), expected, msg);
  is(headers.get(name.toLowerCase()), expected, msg);
  is(headers.get(name.toUpperCase()), expected, msg);
}

function checkGetAll(headers, name, expected, msg) {
  arrayEquals(headers.getAll(name), expected, msg);
  arrayEquals(headers.getAll(name.toLowerCase()), expected, msg);
  arrayEquals(headers.getAll(name.toUpperCase()), expected, msg);
}

//
// Test Cases
//

function TestCoreBehavior(headers, name) {
  var start = headers.getAll(name);

  headers.append(name, "bar");

  var expectedFirst = (start.length ? start[0] : "bar");

  checkHas(headers, name, "Has the header");
  checkGet(headers, name, expectedFirst, "Retrieve first header for name");
  checkGetAll(headers, name, start.concat(["bar"]), "Retrieve all headers for name");

  headers.append(name, "baz");
  checkHas(headers, name, "Has the header");
  checkGet(headers, name, expectedFirst, "Retrieve first header for name");
  checkGetAll(headers, name, start.concat(["bar","baz"]), "Retrieve all headers for name");

  headers.set(name, "snafu");
  checkHas(headers, name, "Has the header after set");
  checkGet(headers, name, "snafu", "Retrieve first header after set");
  checkGetAll(headers, name, ["snafu"], "Retrieve all headers after set");

  headers.delete(name.toUpperCase());
  checkNotHas(headers, name, "Does not have the header after delete");
  checkGet(headers, name, null, "Retrieve first header after delete");
  checkGetAll(headers, name, [], "Retrieve all headers after delete");

  // should be ok to delete non-existent name
  headers.delete(name);

  shouldThrow(function() {
    headers.append("foo,", "bam");
  }, TypeError, "Append invalid header name should throw TypeError.");

  shouldThrow(function() {
    headers.append(name, "bam\n");
  }, TypeError, "Append invalid header value should throw TypeError.");

  shouldThrow(function() {
    headers.append(name, "bam\n\r");
  }, TypeError, "Append invalid header value should throw TypeError.");

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
  checkGetAll(filled, "abc", source.getAll("abc"), "Single value header list matches");
  checkGetAll(filled, "def", source.getAll("def"), "Multiple value header list matches");
  TestCoreBehavior(filled, "def");

  filled = new Headers({
    "zxy": "987",
    "xwv": "654",
    "uts": "321"
  });
  checkGetAll(filled, "zxy", ["987"], "Has first object filled key");
  checkGetAll(filled, "xwv", ["654"], "Has second object filled key");
  checkGetAll(filled, "uts", ["321"], "Has third object filled key");
  TestCoreBehavior(filled, "xwv");

  filled = new Headers([
    ["zxy", "987"],
    ["xwv", "654"],
    ["xwv", "abc"],
    ["uts", "321"]
  ]);
  checkGetAll(filled, "zxy", ["987"], "Has first sequence filled key");
  checkGetAll(filled, "xwv", ["654", "abc"], "Has second sequence filled key");
  checkGetAll(filled, "uts", ["321"], "Has third sequence filled key");
  TestCoreBehavior(filled, "xwv");

  shouldThrow(function() {
    filled = new Headers([
      ["zxy", "987", "654"],
      ["uts", "321"]
    ]);
  }, TypeError, "Fill with non-tuple sequence should throw TypeError.");

  shouldThrow(function() {
    filled = new Headers([
      ["zxy"],
      ["uts", "321"]
    ]);
  }, TypeError, "Fill with non-tuple sequence should throw TypeError.");
}

TestEmptyHeaders();
TestFilledHeaders();
