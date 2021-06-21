function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug234628-2.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u20AC"),
    129,
    "Parent doc should be windows-1252 initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf(
      "\u00E2\u201A\u00AC"
    ),
    78,
    "Child doc should be windows-1252 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u20AC"),
    129,
    "Parent doc should be windows-1252 subsequently"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u20AC"),
    78,
    "Child doc should be UTF-8 subsequently"
  );

  is(
    content.document.characterSet,
    "windows-1252",
    "Parent doc should report windows-1252 subsequently"
  );
  is(
    content.frames[0].document.characterSet,
    "UTF-8",
    "Child doc should report UTF-8 subsequently"
  );
}
