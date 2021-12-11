/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * All images in schema_15_profile.zip are from https://github.com/mdn/sw-test/
 * and are CC licensed by https://www.flickr.com/photos/legofenris/.
 */

async function testSteps() {
  create_test_profile("schema_15_profile.zip");

  const cache = await caches.open("xpcshell-test");
  ok(cache, "cache exists");

  const requestList = await cache.keys();

  ok(requestList.length > 0, "should have at least one request in cache");
  for (const request of requestList) {
    ok(request, "each request in list should be non-null");
    ok(
      request.redirect === "follow",
      'request.redirect should default to "follow"'
    );
    ok(
      request.cache === "default",
      'request.cache should have been updated to "default"' + request.cache
    );
    ok(
      request.mode === "navigate",
      'request.mode should have been updated to "navigate"'
    );
    ok(
      request.referrerPolicy === "no-referrer-when-downgrade",
      'request.referrerPolicy should have been updated to "no-referrer-when-downgrade"'
    );
  }

  const responseList = await Promise.all(
    requestList.map(function(request) {
      return cache.match(request);
    })
  );

  ok(responseList.length > 0, "should have at least one response in cache");
  for (const response of responseList) {
    ok(response, "each response should be non-null");
    // reponse.url is a empty string in current test file. It should test for
    // not being a empty string once thet test file is updated.
    ok(
      typeof response.url === "string",
      "each response.url should be a string"
    );
    // reponse.redirected may be changed once test file is updated. It should
    // be false since current reponse.url is a empty string.
    ok(
      response.redirected === false,
      "each response.redirected should be false"
    );
    Assert.equal(
      response.headers.get("Content-Type"),
      "text/plain;charset=UTF-8",
      "the response should have the correct header"
    );
  }
}
