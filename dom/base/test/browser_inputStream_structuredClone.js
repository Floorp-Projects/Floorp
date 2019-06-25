/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URIs = [
  "about:about",
  "http://example.com/browser/dom/base/test/empty.html"
];

async function runTest(input, url) {
  let tab = BrowserTestUtils.addTab(gBrowser, url);
  let browser = gBrowser.getBrowserForTab(tab);

  await BrowserTestUtils.browserLoaded(browser);

  let stream = Cc['@mozilla.org/io/string-input-stream;1']
               .createInstance(Ci.nsIStringInputStream);
  stream.setData(input, input.length);

  let data = {
    inputStream: stream
  };

  is(data.inputStream.available(), input.length, "The length of the inputStream matches: " + input.length);

  /* eslint-disable no-shadow */
  let dataBack = await ContentTask.spawn(browser, data, function(data) {
    let dataBack = {
      inputStream: data.inputStream,
      check: true,
    };

    if (content.location.href.startsWith('about:')) {
      dataBack.check = data.inputStream instanceof content.Components.interfaces.nsIInputStream;
    }

    return dataBack;
  });
  /* eslint-enable no-shadow */


  ok(dataBack.check, "The inputStream is a nsIInputStream also on content.");
  ok(data.inputStream instanceof Ci.nsIInputStream, "The original object was an inputStream");
  ok(dataBack.inputStream instanceof Ci.nsIInputStream, "We have an inputStream back from the content.");

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test() {
  let a = "a";
  for (let i = 0; i < 25; ++i) {
    a+=a;
  }

  for (let i = 0; i < URIs.length; ++i) {
    await runTest("Hello world", URIs[i]);
    await runTest(a, URIs[i]);
  }
});
