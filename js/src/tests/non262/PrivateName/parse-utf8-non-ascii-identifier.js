// Test that non-ASCII identifier names are correctly parsed in the Utf-8 parser.

// Utf-8 encoding for U+05EF is (0xD7 0xAF), the first code unit isn't a valid
// Ascii ID_START code unit.
class NonAscii {
  // U+05EF HEBREW YOD TRIANGLE
  #ׯ;
}

// Also check using Unicode escapes works.
class NonAsciiUnicodeEscape1 {
  // U+05EF HEBREW YOD TRIANGLE
  #\u05ef;
}

class NonAsciiUnicodeEscape2 {
  // U+05EF HEBREW YOD TRIANGLE
  #\u{5ef};
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
