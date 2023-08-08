/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that the url formatter properly recognizes the host and de-emphasizes
// the rest of the url.

/**
 * Tests a given url.
 * The de-emphasized parts must be wrapped in "<" and ">" chars.
 *
 * @param {string} urlFormatString The URL to test.
 * @param {string} [clobberedURLString] Normally the URL is de-emphasized
 *        in-place, thus it's enough to pass aExpected. Though, in some cases
 *        the formatter may decide to replace the URL with a fixed one, because
 *        it can't properly guess a host. In that case clobberedURLString is
 *        the expected de-emphasized value.
 */
async function testVal(urlFormatString, clobberedURLString = null) {
  let str = urlFormatString.replace(/[<>]/g, "");

  info("Setting the value property directly");
  gURLBar.value = str;
  gBrowser.selectedBrowser.focus();
  UrlbarTestUtils.checkFormatting(window, urlFormatString, {
    clobberedURLString,
  });

  info("Simulating user input");
  await UrlbarTestUtils.inputIntoURLBar(window, str);
  Assert.equal(
    gURLBar.editor.rootElement.textContent,
    str,
    "URL is not highlighted"
  );
  gBrowser.selectedBrowser.focus();
  UrlbarTestUtils.checkFormatting(window, urlFormatString, {
    clobberedURLString,
    additionalMsg: "with input simulation",
  });
}

add_task(async function () {
  const PREF_FORMATTING = "browser.urlbar.formatting.enabled";
  const PREF_TRIM_HTTPS = "browser.urlbar.trimHttps";

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_FORMATTING);
    Services.prefs.clearUserPref(PREF_TRIM_HTTPS);
    gURLBar.setURI();
  });

  Services.prefs.setBoolPref(PREF_TRIM_HTTPS, false);

  gBrowser.selectedBrowser.focus();

  await testVal("<https://>mozilla.org");
  await testVal("<https://>mözilla.org");
  await testVal("<https://>mozilla.imaginatory");

  await testVal("<https://www.>mozilla.org");
  await testVal("<https://sub.>mozilla.org");
  await testVal("<https://sub1.sub2.sub3.>mozilla.org");
  await testVal("<www.>mozilla.org");
  await testVal("<sub.>mozilla.org");
  await testVal("<sub1.sub2.sub3.>mozilla.org");
  await testVal("<mozilla.com.>mozilla.com");
  await testVal("<https://mozilla.com:mozilla.com@>mozilla.com");
  await testVal("<mozilla.com:mozilla.com@>mozilla.com");

  await testVal("<ftp.>mozilla.org");
  await testVal("<ftp://ftp.>mozilla.org");

  await testVal("<https://sub.>mozilla.org");
  await testVal("<https://sub1.sub2.sub3.>mozilla.org");
  await testVal("<https://user:pass@sub1.sub2.sub3.>mozilla.org");
  await testVal("<https://user:pass@>mozilla.org");
  await testVal("<user:pass@sub1.sub2.sub3.>mozilla.org");
  await testVal("<user:pass@>mozilla.org");

  await testVal("<https://>mozilla.org<   >");
  await testVal("mozilla.org<   >");
  // RTL characters in domain change order of domain and suffix. Domain should
  // be highlighted correctly.
  await testVal("<http://>اختبار.اختبار</www.mozilla.org/index.html>");

  await testVal("<https://>mozilla.org</file.ext>");
  await testVal("<https://>mozilla.org</sub/file.ext>");
  await testVal("<https://>mozilla.org</sub/file.ext?foo>");
  await testVal("<https://>mozilla.org</sub/file.ext?foo&bar>");
  await testVal("<https://>mozilla.org</sub/file.ext?foo&bar#top>");
  await testVal("<https://>mozilla.org</sub/file.ext?foo&bar#top>");
  await testVal("foo.bar<?q=test>");
  await testVal("foo.bar<#mozilla.org>");
  await testVal("foo.bar<?somewhere.mozilla.org>");
  await testVal("foo.bar<?@mozilla.org>");
  await testVal("foo.bar<#x@mozilla.org>");
  await testVal("foo.bar<#@x@mozilla.org>");
  await testVal("foo.bar<?x@mozilla.org>");
  await testVal("foo.bar<?@x@mozilla.org>");
  await testVal("<foo.bar@x@>mozilla.org");
  await testVal("<foo.bar@:baz@>mozilla.org");
  await testVal("<foo.bar:@baz@>mozilla.org");
  await testVal("<foo.bar@:ba:z@>mozilla.org");
  await testVal("<foo.:bar:@baz@>mozilla.org");
  await testVal(
    "foopy:\\blah@somewhere.com//whatever/",
    "foopy</blah@somewhere.com//whatever/>"
  );

  await testVal("<https://sub.>mozilla.org<:666/file.ext>");
  await testVal("<sub.>mozilla.org<:666/file.ext>");
  await testVal("localhost<:666/file.ext>");

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
  for (let IP of IPs) {
    await testVal(IP);
    await testVal(IP + "</file.ext>");
    await testVal(IP + "<:666/file.ext>");
    await testVal("<https://>" + IP);
    await testVal(`<https://>${IP}</file.ext>`);
    await testVal(`<https://user:pass@>${IP}<:666/file.ext>`);
    await testVal(`<user:pass@>${IP}<:666/file.ext>`);
    await testVal(`user:\\pass@${IP}/`, `user</pass@${IP}/>`);
  }

  await testVal("mailto:admin@mozilla.org");
  await testVal("gopher://mozilla.org/");
  await testVal("about:config");
  await testVal("jar:http://mozilla.org/example.jar!/");
  await testVal("view-source:http://mozilla.org/");
  await testVal("foo9://mozilla.org/");
  await testVal("foo+://mozilla.org/");
  await testVal("foo.://mozilla.org/");
  await testVal("foo-://mozilla.org/");

  // Disable formatting.
  Services.prefs.setBoolPref(PREF_FORMATTING, false);

  await testVal("https://mozilla.org");
});
