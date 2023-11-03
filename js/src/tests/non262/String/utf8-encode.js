// |reftest| skip-if(!xulRuntime.shell)

var BUGNUMBER = 1561567;
var summary = 'JS_EncodeStringToUTF8BufferPartial - Encode string as UTF-8 into a byte array';

print(BUGNUMBER + ": " + summary);

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

assertEq(isSameCompartment(newRope, this), true);

function checkUtf8Equal(first, second) {
  var firstBuffer = new Uint8Array(first.length * 3);
  var secondBuffer = new Uint8Array(second.length * 3);

  var [firstRead, firstWritten] = encodeAsUtf8InBuffer(first, firstBuffer);
  var [secondRead, secondWritten] = encodeAsUtf8InBuffer(second, secondBuffer);

  assertEq(first.length, firstRead);
  assertEq(second.length, secondRead);

  assertEq(firstWritten, secondWritten);

  for (var i = 0; i < firstWritten; ++i) {
    assertEq(firstBuffer[i], secondBuffer[i]);
  }
}

concat.forEach(function(t) {
  var filler = "012345678901234567890123456789";
  var rope = newRope(t.head, newRope(t.tail, filler));
  checkUtf8Equal(rope, t.expected + filler);
});

{
  var filler = "012345678901234567890123456789";

  var a = newRope(filler, "a");
  var ab = newRope(a, "b");
  var abc = newRope(ab, "c");

  var e = newRope(filler, "e");
  var ef = newRope(e, "f");
  var def = newRope("d", ef);

  var abcdef = newRope(abc, def);
  var abcdefab = newRope(abcdef, ab);
  checkUtf8Equal(
    abcdefab,
    "012345678901234567890123456789abcd012345678901234567890123456789ef012345678901234567890123456789ab"
  );
}

{
  var filler = "012345678901234567890123456789";

  var right = newRope("\ude0a", filler);
  var rope = newRope("\ud83d", right);
  checkUtf8Equal(rope, "\ud83d\ude0a012345678901234567890123456789");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
