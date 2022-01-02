function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug234628-5.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u20AC"),
    146,
    "Parent doc should be windows-1252 initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u20AC"),
    87,
    "Child doc should be utf-16 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u20AC"),
    146,
    "Parent doc should be windows-1252 subsequently"
  );
  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u20AC"),
    87,
    "Child doc should decode as utf-16 subsequently"
  );

  is(
    content.document.characterSet,
    "windows-1252",
    "Parent doc should report windows-1252 subsequently"
  );
  is(
    content.frames[0].document.characterSet,
    "UTF-16LE",
    "Child doc should report UTF-16LE subsequently"
  );
}
