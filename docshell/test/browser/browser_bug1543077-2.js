function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1543077-2.html",
    afterOpen,
    "Japanese",
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u201A"),
    134,
    "Parent doc should be windows-1252 initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u201A"),
    90,
    "Child doc should be windows-1252 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u3042"),
    134,
    "Parent doc should decode as Shift_JIS subsequently"
  );
  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u3042"),
    90,
    "Child doc should decode as Shift_JIS subsequently"
  );

  is(
    content.document.characterSet,
    "Shift_JIS",
    "Parent doc should report Shift_JIS subsequently"
  );
  is(
    content.frames[0].document.characterSet,
    "Shift_JIS",
    "Child doc should report Shift_JIS subsequently"
  );
}
