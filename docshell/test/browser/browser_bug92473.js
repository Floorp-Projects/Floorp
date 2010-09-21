/* The test text decoded correctly as Shift_JIS */
const rightText="\u30E6\u30CB\u30B3\u30FC\u30C9\u306F\u3001\u3059\u3079\u3066\u306E\u6587\u5B57\u306B\u56FA\u6709\u306E\u756A\u53F7\u3092\u4ED8\u4E0E\u3057\u307E\u3059";

/* The test text decoded incorrectly as Windows-1251. This is the "right" wrong
   text; anything else is unexpected. */
const wrongText="\u0453\u2020\u0453\u006A\u0453\u0052\u0403\u005B\u0453\u0068\u201A\u041D\u0403\u0041\u201A\u00B7\u201A\u0427\u201A\u0414\u201A\u041C\u2022\u00B6\u040B\u0459\u201A\u0419\u040A\u0415\u2014\u004C\u201A\u041C\u201D\u0424\u040C\u2020\u201A\u0440\u2022\u0074\u2014\u005E\u201A\u00B5\u201A\u042C\u201A\u00B7";

var testPage;

function url(spec) {
  return Services.io.newURI(spec, null, null);
}

function testContent(text) {
    is(testPage.document.getElementById("testpar").innerHTML, text,
       "<p> contains expected text");
    is(testPage.document.getElementById("testtextarea").innerHTML, text,
       "<textarea> contains expected text");
    is(testPage.document.getElementById("testinput").value, text,
       "<input> contains expected text");
}

function afterOpen() {
    testPage.events.removeListener("load", afterOpen);
    testPage.events.addListener("load", afterChangeCharset);
    /* Test that the content on load is the expected wrong decoding */
    testContent(wrongText);

    /* Force the page encoding to Shift_JIS */
    SetForcedCharset("Shift_JIS");
}
  
function afterChangeCharset() {
    testPage.events.removeListener("load", afterChangeCharset);
    /* test that the content is decoded correctly */
    testContent(rightText);
    testPage.close();
    finish();
}

function test() {
  var activeWin = Application.activeWindow;
  testPage = activeWin.open(url("about:blank"));
  testPage.events.addListener("load", afterOpen);
  
  var rootDir = getRootDirectory(gTestPath);
  testPage.load(url(rootDir + "test-form_sjis.html"));
  testPage.focus();

  waitForExplicitFinish();
}
