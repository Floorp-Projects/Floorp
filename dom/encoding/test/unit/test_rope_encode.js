var concat = [
  {
    head: "a",
    tail: "b",
    expected: "ab",
    name: "Latin1 and Latin1",
  },
  {
    head: "α",
    tail: "β",
    expected: "αβ",
    name: "UTF-16 and UTF-16",
  },
  {
    head: "a",
    tail: "β",
    expected: "aβ",
    name: "Latin1 and UTF-16",
  },
  {
    head: "α",
    tail: "b",
    expected: "αb",
    name: "UTF-16 and Latin1",
  },
  {
    head: "\uD83D",
    tail: "\uDE03",
    expected: "\uD83D\uDE03",
    name: "Surrogate pair",
  },
  {
    head: "a\uD83D",
    tail: "\uDE03b",
    expected: "a\uD83D\uDE03b",
    name: "Surrogate pair with prefix and suffix",
  },
  {
    head: "\uD83D",
    tail: "b",
    expected: "\uFFFDb",
    name: "Unpaired high surrogate and Latin1",
  },
  {
    head: "a\uD83D",
    tail: "b",
    expected: "a\uFFFDb",
    name: "Prefixed unpaired high surrogate and Latin1",
  },
  {
    head: "\uD83D",
    tail: "β",
    expected: "\uFFFDβ",
    name: "Unpaired high surrogate and UTF-16",
  },
  {
    head: "a\uD83D",
    tail: "β",
    expected: "a\uFFFDβ",
    name: "Prefixed unpaired high surrogate and UTF-16",
  },

  {
    head: "\uDE03",
    tail: "b",
    expected: "\uFFFDb",
    name: "Unpaired low surrogate and Latin1",
  },
  {
    head: "a\uDE03",
    tail: "b",
    expected: "a\uFFFDb",
    name: "Prefixed unpaired low surrogate and Latin1",
  },
  {
    head: "\uDE03",
    tail: "β",
    expected: "\uFFFDβ",
    name: "Unpaired low surrogate and UTF-16",
  },
  {
    head: "a\uDE03",
    tail: "β",
    expected: "a\uFFFDβ",
    name: "Prefixed unpaired low surrogate and UTF-16",
  },

  {
    head: "a",
    tail: "\uDE03",
    expected: "a\uFFFD",
    name: "Latin1 and unpaired low surrogate",
  },
  {
    head: "a",
    tail: "\uDE03b",
    expected: "a\uFFFDb",
    name: "Latin1 and suffixed unpaired low surrogate",
  },
  {
    head: "α",
    tail: "\uDE03",
    expected: "α\uFFFD",
    name: "UTF-16 and unpaired low surrogate",
  },
  {
    head: "α",
    tail: "\uDE03b",
    expected: "α\uFFFDb",
    name: "UTF-16 and suffixed unpaired low surrogate",
  },

  {
    head: "a",
    tail: "\uD83D",
    expected: "a\uFFFD",
    name: "Latin1 and unpaired high surrogate",
  },
  {
    head: "a",
    tail: "\uD83Db",
    expected: "a\uFFFDb",
    name: "Latin1 and suffixed unpaired high surrogate",
  },
  {
    head: "α",
    tail: "\uD83D",
    expected: "α\uFFFD",
    name: "UTF-16 and unpaired high surrogate",
  },
  {
    head: "α",
    tail: "\uD83Db",
    expected: "α\uFFFDb",
    name: "UTF-16 and suffixed unpaired high surrogate",
  },
];

var testingFunctions = Cu.getJSTestingFunctions();
concat.forEach(function (t) {
  test(function () {
    assert_true(
      testingFunctions.isSameCompartment(testingFunctions.newRope, this),
      "Must be in the same compartment"
    );
    var filler = "012345678901234567890123456789";
    var rope = testingFunctions.newRope(
      t.head,
      testingFunctions.newRope(t.tail, filler)
    );
    var encoded = new TextEncoder().encode(rope);
    var decoded = new TextDecoder().decode(encoded);
    assert_equals(decoded, t.expected + filler, "Must round-trip");
  }, t.name);
});

test(function () {
  assert_true(
    testingFunctions.isSameCompartment(testingFunctions.newRope, this),
    "Must be in the same compartment"
  );

  var filler = "012345678901234567890123456789";

  var a = testingFunctions.newRope(filler, "a");
  var ab = testingFunctions.newRope(a, "b");
  var abc = testingFunctions.newRope(ab, "c");

  var e = testingFunctions.newRope(filler, "e");
  var ef = testingFunctions.newRope(e, "f");
  var def = testingFunctions.newRope("d", ef);

  var abcdef = testingFunctions.newRope(abc, def);
  var abcdefab = testingFunctions.newRope(abcdef, ab);

  var encoded = new TextEncoder().encode(abcdefab);
  var decoded = new TextDecoder().decode(encoded);
  assert_equals(
    decoded,
    "012345678901234567890123456789abcd012345678901234567890123456789ef012345678901234567890123456789ab",
    "Must walk the DAG correctly"
  );
}, "Complex rope DAG");
