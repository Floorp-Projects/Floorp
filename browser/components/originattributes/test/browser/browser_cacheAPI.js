const requestURL = "https://test1.example.com";

function getResult(aBrowser) {
  return SpecialPowers.spawn(aBrowser, [requestURL], async function (url) {
    let cache = await content.caches.open("TEST_CACHE");
    let response = await cache.match(url);
    if (response) {
      return response.statusText;
    }
    let result = Math.random().toString();
    response = new content.Response("", { statusText: result });
    await cache.put(url, response);
    return result;
  });
}

IsolationTestTools.runTests(
  "https://test2.example.com",
  getResult,
  null,
  null,
  false,
  /* aUseHttps */ true
);
