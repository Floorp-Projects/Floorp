function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1648464-1.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u00A4"),
    146,
    "Parent doc should be windows-1252 initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u00A4"),
    95,
    "Child doc should be windows-1252 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u3042"),
    146,
    "Parent doc should decode as EUC-JP subsequently"
  );
  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u3042"),
    95,
    "Child doc should decode as EUC-JP subsequently"
  );

  is(
    content.document.characterSet,
    "EUC-JP",
    "Parent doc should report EUC-JP subsequently"
  );
  is(
    content.frames[0].document.characterSet,
    "EUC-JP",
    "Child doc should report EUC-JP subsequently"
  );
}
