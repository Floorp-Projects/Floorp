/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that the url formatter properly recognizes the host and de-emphasizes
// the rest of the url.

/**
 * Tests a given url.
 * The de-emphasized parts must be wrapped in "<" and ">" chars.
 * @param {string} aExpected The url to test.
 * @param {string} aClobbered [optional] Normally the url is de-emphasized
 *        in-place, thus it's enough to pass aExpected. Though, in some cases
 *        the formatter may decide to replace the url with a fixed one, because
 *        it can't properly guess a host. In that case aClobbered is the
 *        expected de-emphasized value.
 * @param {boolean} synthesizeInput [optional] Whether to synthesize an input
 *        event to test.
 */
function testVal(aExpected, aClobbered = null, synthesizeInput = false) {
  let str = aExpected.replace(/[<>]/g, "");
  if (synthesizeInput) {
    gURLBar.focus();
    gURLBar.select();
    EventUtils.sendString(str);
    Assert.equal(
      gURLBar.editor.rootElement.textContent,
      str,
      "Url is not highlighted"
    );
    gBrowser.selectedBrowser.focus();
  } else {
    gURLBar.value = str;
  }

  let selectionController = gURLBar.editor.selectionController;
  let selection = selectionController.getSelection(
    selectionController.SELECTION_URLSECONDARY
  );
  let value = gURLBar.editor.rootElement.textContent;
  let result = "";
  for (let i = 0; i < selection.rangeCount; i++) {
    let range = selection.getRangeAt(i).toString();
    let pos = value.indexOf(range);
    result += value.substring(0, pos) + "<" + range + ">";
    value = value.substring(pos + range.length);
  }
  result += value;
  Assert.equal(
    result,
    aClobbered || aExpected,
    "Correct part of the url is de-emphasized" +
      (synthesizeInput ? " (with input simulation)" : "")
  );

  // Now re-test synthesizing input.
  if (!synthesizeInput) {
    testVal(aExpected, aClobbered, true);
  }
}

function test() {
  const prefname = "browser.urlbar.formatting.enabled";

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(prefname);
    gURLBar.setURI();
  });

  gBrowser.selectedBrowser.focus();

  testVal("<https://>mozilla.org");
  testVal("<https://>mözilla.org");
  testVal("<https://>mozilla.imaginatory");

  testVal("<https://www.>mozilla.org");
  testVal("<https://sub.>mozilla.org");
  testVal("<https://sub1.sub2.sub3.>mozilla.org");
  testVal("<www.>mozilla.org");
  testVal("<sub.>mozilla.org");
  testVal("<sub1.sub2.sub3.>mozilla.org");
  testVal("<mozilla.com.>mozilla.com");
  testVal("<https://mozilla.com:mozilla.com@>mozilla.com");
  testVal("<mozilla.com:mozilla.com@>mozilla.com");

  testVal("<ftp.>mozilla.org");
  testVal("<ftp://ftp.>mozilla.org");

  testVal("<https://sub.>mozilla.org");
  testVal("<https://sub1.sub2.sub3.>mozilla.org");
  testVal("<https://user:pass@sub1.sub2.sub3.>mozilla.org");
  testVal("<https://user:pass@>mozilla.org");
  testVal("<user:pass@sub1.sub2.sub3.>mozilla.org");
  testVal("<user:pass@>mozilla.org");

  testVal("<https://>mozilla.org<   >");
  testVal("mozilla.org<   >");

  testVal("<https://>mozilla.org</file.ext>");
  testVal("<https://>mozilla.org</sub/file.ext>");
  testVal("<https://>mozilla.org</sub/file.ext?foo>");
  testVal("<https://>mozilla.org</sub/file.ext?foo&bar>");
  testVal("<https://>mozilla.org</sub/file.ext?foo&bar#top>");
  testVal("<https://>mozilla.org</sub/file.ext?foo&bar#top>");
  testVal("foo.bar<?q=test>");
  testVal("foo.bar<#mozilla.org>");
  testVal("foo.bar<?somewhere.mozilla.org>");
  testVal("foo.bar<?@mozilla.org>");
  testVal("foo.bar<#x@mozilla.org>");
  testVal("foo.bar<#@x@mozilla.org>");
  testVal("foo.bar<?x@mozilla.org>");
  testVal("foo.bar<?@x@mozilla.org>");
  testVal("<foo.bar@x@>mozilla.org");
  testVal("<foo.bar@:baz@>mozilla.org");
  testVal("<foo.bar:@baz@>mozilla.org");
  testVal("<foo.bar@:ba:z@>mozilla.org");
  testVal("<foo.:bar:@baz@>mozilla.org");
  testVal(
    "foopy:\\blah@somewhere.com//whatever/",
    "foopy</blah@somewhere.com//whatever/>"
  );

  testVal("<https://sub.>mozilla.org<:666/file.ext>");
  testVal("<sub.>mozilla.org<:666/file.ext>");
  testVal("localhost<:666/file.ext>");

  let IPs = [
    "192.168.1.1",
    "[::]",
    "[::1]",
    "[1::]",
    "[::]",
    "[::1]",
    "[1::]",
    "[1:2:3:4:5:6:7::]",
    "[::1:2:3:4:5:6:7]",
    "[1:2:a:B:c:D:e:F]",
    "[1::8]",
    "[1:2::8]",
    "[fe80::222:19ff:fe11:8c76]",
    "[0000:0123:4567:89AB:CDEF:abcd:ef00:0000]",
    "[::192.168.1.1]",
    "[1::0.0.0.0]",
    "[1:2::255.255.255.255]",
    "[1:2:3::255.255.255.255]",
    "[1:2:3:4::255.255.255.255]",
    "[1:2:3:4:5::255.255.255.255]",
    "[1:2:3:4:5:6:255.255.255.255]",
  ];
  IPs.forEach(function(IP) {
    testVal(IP);
    testVal(IP + "</file.ext>");
    testVal(IP + "<:666/file.ext>");
    testVal("<https://>" + IP);
    testVal(`<https://>${IP}</file.ext>`);
    testVal(`<https://user:pass@>${IP}<:666/file.ext>`);
    testVal(`<user:pass@>${IP}<:666/file.ext>`);
    testVal(`user:\\pass@${IP}/`, `user</pass@${IP}/>`);
  });

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");
  testVal("jar:http://mozilla.org/example.jar!/");
  testVal("view-source:http://mozilla.org/");
  testVal("foo9://mozilla.org/");
  testVal("foo+://mozilla.org/");
  testVal("foo.://mozilla.org/");
  testVal("foo-://mozilla.org/");

  // Disable formatting.
  Services.prefs.setBoolPref(prefname, false);

  testVal("https://mozilla.org");
}
