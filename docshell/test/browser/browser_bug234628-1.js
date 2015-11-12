function test() {
  var rootDir = "http://mochi.test:8888/browser/docshell/test/browser/";
  runCharsetTest(rootDir + "file_bug234628-1.html", afterOpen, "windows-1251", afterChangeCharset);
}

function afterOpen() {
  is(content.document.documentElement.textContent.indexOf('\u20AC'), 129, "Parent doc should be windows-1252 initially");

  is(content.frames[0].document.documentElement.textContent.indexOf('\u20AC'), 85, "Child doc should be windows-1252 initially");
}

function afterChangeCharset() {
  is(content.document.documentElement.textContent.indexOf('\u0402'), 129, "Parent doc should decode as windows-1251 subsequently");
  is(content.frames[0].document.documentElement.textContent.indexOf('\u0402'), 85, "Child doc should decode as windows-1251 subsequently");

  is(content.document.characterSet, "windows-1251", "Parent doc should report windows-1251 subsequently");
  is(content.frames[0].document.characterSet, "windows-1251", "Child doc should report windows-1251 subsequently");
}
