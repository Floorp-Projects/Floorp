function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1543077-1.html",
    afterOpen,
    "Japanese",
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u0434"),
    131,
    "Parent doc should be IBM866 initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u0434"),
    87,
    "Child doc should be IBM866 initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u3042"),
    131,
    "Parent doc should decode as EUC-JP subsequently"
  );
  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u3042"),
    87,
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
