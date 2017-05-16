/* The test text as octets for reference
 * %83%86%83%6a%83%52%81%5b%83%68%82%cd%81%41%82%b7%82%d7%82%c4%82%cc%95%b6%8e%9a%82%c9%8c%c5%97%4c%82%cc%94%d4%8d%86%82%f0%95%74%97%5e%82%b5%82%dc%82%b7
 */

function testContent(text) {
  return ContentTask.spawn(gBrowser.selectedBrowser, text, text => {
    Assert.equal(content.document.getElementById("testpar").innerHTML, text,
      "<p> contains expected text");
    Assert.equal(content.document.getElementById("testtextarea").innerHTML, text,
      "<textarea> contains expected text");
    Assert.equal(content.document.getElementById("testinput").value, text,
      "<input> contains expected text");
  });
}

function afterOpen() {
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(afterChangeCharset);

  /* The test text decoded incorrectly as Windows-1251. This is the "right" wrong
   text; anything else is unexpected. */
  const wrongText="\u0453\u2020\u0453\u006A\u0453\u0052\u0403\u005B\u0453\u0068\u201A\u041D\u0403\u0041\u201A\u00B7\u201A\u0427\u201A\u0414\u201A\u041C\u2022\u00B6\u040B\u0459\u201A\u0419\u040A\u0415\u2014\u004C\u201A\u041C\u201D\u0424\u040C\u2020\u201A\u0440\u2022\u0074\u2014\u005E\u201A\u00B5\u201A\u042C\u201A\u00B7";

  /* Test that the content on load is the expected wrong decoding */
  testContent(wrongText).then(() => {
    BrowserSetForcedCharacterSet("Shift_JIS");
  });
}

function afterChangeCharset() {
  /* The test text decoded correctly as Shift_JIS */
  const rightText="\u30E6\u30CB\u30B3\u30FC\u30C9\u306F\u3001\u3059\u3079\u3066\u306E\u6587\u5B57\u306B\u56FA\u6709\u306E\u756A\u53F7\u3092\u4ED8\u4E0E\u3057\u307E\u3059";

  /* test that the content is decoded correctly */
  testContent(rightText).then(() => {
    gBrowser.removeCurrentTab();
    finish();
  });
}

function test() {
  waitForExplicitFinish();

  // Get the local directory. This needs to be a file: URI because chrome: URIs
  // are always UTF-8 (bug 617339) and we are testing decoding from other
  // charsets.
  var jar = getJar(getRootDirectory(gTestPath));
  var dir = jar ?
              extractJarToTmp(jar) :
              getChromeDir(getResolvedURI(gTestPath));
  var rootDir = Services.io.newFileURI(dir).spec;

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, rootDir + "test-form_sjis.html");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(afterOpen);
}
