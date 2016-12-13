const requestURL = "https://test1.example.com";

function getResult(aBrowser) {
  return ContentTask.spawn(aBrowser, requestURL, function* (url) {
    let cache = yield content.caches.open("TEST_CACHE");
    let response = yield cache.match(url);
    if (response) {
      return response.statusText;
    }
    let result = Math.random().toString();
    response = new content.Response("", { statusText: result });
    yield cache.put(url, response);
    return result;
  });
}

IsolationTestTools.runTests("https://test2.example.com", getResult, null, null,
                            false, /* aUseHttps */ true);
