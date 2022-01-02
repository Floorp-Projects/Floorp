function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(
    rootDir + "file_bug1543077-3.html",
    afterOpen,
    afterChangeCharset
  );
}

function afterOpen() {
  is(
    content.document.documentElement.textContent.indexOf("\u3042"),
    136,
    "Parent doc should be ISO-2022-JP initially"
  );

  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u3042"),
    92,
    "Child doc should be ISO-2022-JP initially"
  );
}

function afterChangeCharset() {
  is(
    content.document.documentElement.textContent.indexOf("\u3042"),
    136,
    "Parent doc should decode as ISO-2022-JP subsequently"
  );
  is(
    content.frames[0].document.documentElement.textContent.indexOf("\u3042"),
    92,
    "Child doc should decode as ISO-2022-JP subsequently"
  );

  is(
    content.document.characterSet,
    "ISO-2022-JP",
    "Parent doc should report ISO-2022-JP subsequently"
  );
  is(
    content.frames[0].document.characterSet,
    "ISO-2022-JP",
    "Child doc should report ISO-2022-JP subsequently"
  );
}
