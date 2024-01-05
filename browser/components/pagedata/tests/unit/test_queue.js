/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

// Test that urls are retrieved in the expected order.
add_task(async function test_queueOrder() {
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 0);
  // Pretend we are idle.
  PageDataService.observe(null, "idle", null);

  let pageDataResults = [
    {
      date: Date.now(),
      url: "http://www.mozilla.org/1",
      siteName: "Mozilla",
      data: {},
    },
    {
      date: Date.now() - 3600,
      url: "http://www.google.com/2",
      siteName: "Google",
      data: {},
    },
    {
      date: Date.now() + 3600,
      url: "http://www.example.com/3",
      image: "http://www.example.com/banner.jpg",
      data: {},
    },
    {
      date: Date.now() / 2,
      url: "http://www.wikipedia.org/4",
      data: {},
    },
    {
      date: Date.now() / 3,
      url: "http://www.microsoft.com/5",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Windows 11",
        },
      },
    },
  ];

  let requests = [];
  PageDataService.fetchPageData = url => {
    requests.push(url);

    for (let pageData of pageDataResults) {
      if (pageData.url == url) {
        return Promise.resolve(pageData);
      }
    }

    return Promise.reject(new Error("Unknown url"));
  };

  let { promise: completePromise, resolve } = Promise.withResolvers();

  let results = [];
  let listener = (_, pageData) => {
    results.push(pageData);
    if (results.length == pageDataResults.length) {
      resolve();
    }
  };

  PageDataService.on("page-data", listener);

  for (let pageData of pageDataResults) {
    PageDataService.queueFetch(pageData.url);
  }

  await completePromise;
  PageDataService.off("page-data", listener);

  Assert.deepEqual(
    requests,
    pageDataResults.map(pd => pd.url)
  );

  // Because our fetch implementation is essentially synchronous the results
  // will be in a known order. This isn't guaranteed by the API though.
  Assert.deepEqual(results, pageDataResults);

  delete PageDataService.fetchPageData;
});

// Tests that limiting the number of fetches works.
add_task(async function test_queueLimit() {
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 3);
  // Pretend we are idle.
  PageDataService.observe(null, "idle", null);

  let requests = [];
  PageDataService.fetchPageData = url => {
    let { promise, resolve, reject } = Promise.withResolvers();
    requests.push({ url, resolve, reject });

    return promise;
  };

  let results = [];
  let listener = (_, pageData) => {
    results.push(pageData?.url);
  };

  PageDataService.on("page-data", listener);

  PageDataService.queueFetch("https://www.mozilla.org/1");
  PageDataService.queueFetch("https://www.mozilla.org/2");
  PageDataService.queueFetch("https://www.mozilla.org/3");
  PageDataService.queueFetch("https://www.mozilla.org/4");
  PageDataService.queueFetch("https://www.mozilla.org/5");
  PageDataService.queueFetch("https://www.mozilla.org/6");
  PageDataService.queueFetch("https://www.mozilla.org/7");
  PageDataService.queueFetch("https://www.mozilla.org/8");
  PageDataService.queueFetch("https://www.mozilla.org/9");
  PageDataService.queueFetch("https://www.mozilla.org/10");
  PageDataService.queueFetch("https://www.mozilla.org/11");

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
    ]
  );

  // Completing or rejecting a request should start new ones.

  requests[1].resolve({
    date: 2345,
    url: "https://www.mozilla.org/2",
    siteName: "Test 2",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
    ]
  );

  requests[3].reject(new Error("Fail"));

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
    ]
  );

  // Increasing the limit should start more requests.
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 5);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
    ]
  );

  // Dropping the limit shouldn't start anything new.
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 3);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
    ]
  );

  // But resolving should also not start new requests.
  requests[5].resolve({
    date: 345334,
    url: "https://www.mozilla.org/6",
    siteName: "Test 6",
    data: {},
  });

  requests[0].resolve({
    date: 343446434,
    url: "https://www.mozilla.org/1",
    siteName: "Test 1",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
    ]
  );

  // Until a previous request completes.
  requests[4].resolve(null);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
      "https://www.mozilla.org/8",
    ]
  );

  // Inifinite queue should work.
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 0);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
      "https://www.mozilla.org/8",
      "https://www.mozilla.org/9",
      "https://www.mozilla.org/10",
      "https://www.mozilla.org/11",
    ]
  );

  requests[10].resolve({
    date: 345334,
    url: "https://www.mozilla.org/11",
    data: {},
  });
  requests[2].resolve({
    date: 345334,
    url: "https://www.mozilla.org/3",
    data: {},
  });
  requests[7].resolve({
    date: 345334,
    url: "https://www.mozilla.org/8",
    data: {},
  });
  requests[6].resolve({
    date: 345334,
    url: "https://www.mozilla.org/7",
    data: {},
  });
  requests[8].resolve({
    date: 345334,
    url: "https://www.mozilla.org/9",
    data: {},
  });
  requests[9].resolve({
    date: 345334,
    url: "https://www.mozilla.org/10",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
      "https://www.mozilla.org/8",
      "https://www.mozilla.org/9",
      "https://www.mozilla.org/10",
      "https://www.mozilla.org/11",
    ]
  );

  PageDataService.off("page-data", listener);

  delete PageDataService.fetchPageData;

  Assert.deepEqual(results, [
    "https://www.mozilla.org/2",
    "https://www.mozilla.org/6",
    "https://www.mozilla.org/1",
    "https://www.mozilla.org/11",
    "https://www.mozilla.org/3",
    "https://www.mozilla.org/8",
    "https://www.mozilla.org/7",
    "https://www.mozilla.org/9",
    "https://www.mozilla.org/10",
  ]);
});

// Tests that the user idle state stops and starts fetches.
add_task(async function test_idle() {
  Services.prefs.setIntPref("browser.pagedata.maxBackgroundFetches", 3);
  // Pretend we are active.
  PageDataService.observe(null, "active", null);

  let requests = [];
  PageDataService.fetchPageData = url => {
    let { promise, resolve, reject } = Promise.withResolvers();
    requests.push({ url, resolve, reject });

    return promise;
  };

  let results = [];
  let listener = (_, pageData) => {
    results.push(pageData?.url);
  };

  PageDataService.on("page-data", listener);

  PageDataService.queueFetch("https://www.mozilla.org/1");
  PageDataService.queueFetch("https://www.mozilla.org/2");
  PageDataService.queueFetch("https://www.mozilla.org/3");
  PageDataService.queueFetch("https://www.mozilla.org/4");
  PageDataService.queueFetch("https://www.mozilla.org/5");
  PageDataService.queueFetch("https://www.mozilla.org/6");
  PageDataService.queueFetch("https://www.mozilla.org/7");

  await TestUtils.waitForTick();

  // Nothing will start when active.
  Assert.deepEqual(
    requests.map(r => r.url),
    []
  );

  // Pretend we are idle.
  PageDataService.observe(null, "idle", null);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
    ]
  );

  // Completing or rejecting a request should start new ones.

  requests[1].resolve({
    date: 2345,
    url: "https://www.mozilla.org/2",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
    ]
  );

  // But not when active
  PageDataService.observe(null, "active", null);

  requests[3].resolve({
    date: 2345,
    url: "https://www.mozilla.org/4",
    data: {},
  });
  requests[0].resolve({
    date: 2345,
    url: "https://www.mozilla.org/1",
    data: {},
  });
  requests[2].resolve({
    date: 2345,
    url: "https://www.mozilla.org/3",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
    ]
  );

  // Going idle should start more workers
  PageDataService.observe(null, "idle", null);

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
    ]
  );

  requests[4].resolve({
    date: 2345,
    url: "https://www.mozilla.org/5",
    data: {},
  });
  requests[5].resolve({
    date: 2345,
    url: "https://www.mozilla.org/6",
    data: {},
  });
  requests[6].resolve({
    date: 2345,
    url: "https://www.mozilla.org/7",
    data: {},
  });

  await TestUtils.waitForTick();

  Assert.deepEqual(
    requests.map(r => r.url),
    [
      "https://www.mozilla.org/1",
      "https://www.mozilla.org/2",
      "https://www.mozilla.org/3",
      "https://www.mozilla.org/4",
      "https://www.mozilla.org/5",
      "https://www.mozilla.org/6",
      "https://www.mozilla.org/7",
    ]
  );

  PageDataService.off("page-data", listener);

  delete PageDataService.fetchPageData;

  Assert.deepEqual(results, [
    "https://www.mozilla.org/2",
    "https://www.mozilla.org/4",
    "https://www.mozilla.org/1",
    "https://www.mozilla.org/3",
    "https://www.mozilla.org/5",
    "https://www.mozilla.org/6",
    "https://www.mozilla.org/7",
  ]);
});
