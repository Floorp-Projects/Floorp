// Create a two-byte string which has only Latin-1 characters.
var str = newString("12345678901234567890", {twoByte: true});

// Create a segmenter for |str|.
var segmenter = new Intl.Segmenter();
var segments = segmenter.segment(str);
var segment = segments.containing(0);

var obj = {};

// `obj[str]` to atomize the string. This will change |str| to a dependent
// string of the newly created atom. The atom string is allocated as Latin-1,
// because all characters are Latin-1.
assertEq(obj[str], undefined);
