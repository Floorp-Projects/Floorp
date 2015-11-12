function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(rootDir + "file_bug234628-6.html", afterOpen, "windows-1251", afterChangeCharset);
}

function afterOpen() {
  is(content.document.documentElement.textContent.indexOf('\u20AC'), 190, "Parent doc should be windows-1252 initially");

  is(content.frames[0].document.documentElement.textContent.indexOf('\u20AC'), 109, "Child doc should be utf-16 initially");
}

function afterChangeCharset() {
  is(content.document.documentElement.textContent.indexOf('\u0402'), 190, "Parent doc should decode as windows-1251 subsequently");
  is(content.frames[0].document.documentElement.textContent.indexOf('\u20AC'), 109, "Child doc should decode as utf-16 subsequently");

  is(content.document.characterSet, "windows-1251", "Parent doc should report windows-1251 subsequently");
  is(content.frames[0].document.characterSet, "UTF-16BE", "Child doc should report UTF-16 subsequently");
}
