import { classifySite } from "lib/SiteClassifier.jsm";

const FAKE_CLASSIFIER_DATA = [
  {
    type: "hostname-and-params-match",
    criteria: [
      {
        hostname: "hostnameandparams.com",
        params: [
          {
            key: "param1",
            value: "val1",
          },
        ],
      },
    ],
    weight: 300,
  },
  {
    type: "url-match",
    criteria: [{ url: "https://fullurl.com/must/match" }],
    weight: 400,
  },
  {
    type: "params-match",
    criteria: [
      {
        params: [
          {
            key: "param1",
            value: "val1",
          },
          {
            key: "param2",
            value: "val2",
          },
        ],
      },
    ],
    weight: 200,
  },
  {
    type: "params-prefix-match",
    criteria: [
      {
        params: [
          {
            key: "client",
            prefix: "fir",
          },
        ],
      },
    ],
    weight: 200,
  },
  {
    type: "has-params",
    criteria: [
      {
        params: [{ key: "has-param1" }, { key: "has-param2" }],
      },
    ],
    weight: 100,
  },
  {
    type: "search-engine",
    criteria: [
      { sld: "google" },
      { hostname: "bing.com" },
      { hostname: "duckduckgo.com" },
    ],
    weight: 1,
  },
  {
    type: "news-portal",
    criteria: [
      { hostname: "yahoo.com" },
      { hostname: "aol.com" },
      { hostname: "msn.com" },
    ],
    weight: 1,
  },
  {
    type: "social-media",
    criteria: [{ hostname: "facebook.com" }, { hostname: "twitter.com" }],
    weight: 1,
  },
  {
    type: "ecommerce",
    criteria: [{ sld: "amazon" }, { hostname: "ebay.com" }],
    weight: 1,
  },
];

describe("SiteClassifier", () => {
  function RemoteSettings() {
    return {
      get() {
        return Promise.resolve(FAKE_CLASSIFIER_DATA);
      },
    };
  }

  it("should return the right category", async () => {
    assert.equal(
      "hostname-and-params-match",
      await classifySite(
        "https://hostnameandparams.com?param1=val1",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite(
        "https://hostnameandparams.com?param1=val",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite(
        "https://hostnameandparams.com?param=val1",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite("https://hostnameandparams.com", RemoteSettings)
    );
    assert.equal(
      "other",
      await classifySite("https://params.com?param1=val1", RemoteSettings)
    );

    assert.equal(
      "url-match",
      await classifySite("https://fullurl.com/must/match", RemoteSettings)
    );
    assert.equal(
      "other",
      await classifySite("http://fullurl.com/must/match", RemoteSettings)
    );

    assert.equal(
      "params-match",
      await classifySite(
        "https://example.com?param1=val1&param2=val2",
        RemoteSettings
      )
    );
    assert.equal(
      "params-match",
      await classifySite(
        "https://example.com?param1=val1&param2=val2&other=other",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite(
        "https://example.com?param1=val2&param2=val1",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite("https://example.com?param1&param2", RemoteSettings)
    );

    assert.equal(
      "params-prefix-match",
      await classifySite("https://search.com?client=firefox", RemoteSettings)
    );
    assert.equal(
      "params-prefix-match",
      await classifySite("https://search.com?client=fir", RemoteSettings)
    );
    assert.equal(
      "other",
      await classifySite(
        "https://search.com?client=mozillafirefox",
        RemoteSettings
      )
    );

    assert.equal(
      "has-params",
      await classifySite(
        "https://example.com?has-param1=val1&has-param2=val2",
        RemoteSettings
      )
    );
    assert.equal(
      "has-params",
      await classifySite(
        "https://example.com?has-param1&has-param2",
        RemoteSettings
      )
    );
    assert.equal(
      "has-params",
      await classifySite(
        "https://example.com?has-param1&has-param2&other=other",
        RemoteSettings
      )
    );
    assert.equal(
      "other",
      await classifySite("https://example.com?has-param1", RemoteSettings)
    );
    assert.equal(
      "other",
      await classifySite("https://example.com?has-param2", RemoteSettings)
    );

    assert.equal(
      "search-engine",
      await classifySite("https://google.com", RemoteSettings)
    );
    assert.equal(
      "search-engine",
      await classifySite("https://google.de", RemoteSettings)
    );
    assert.equal(
      "search-engine",
      await classifySite("http://bing.com/?q=firefox", RemoteSettings)
    );

    assert.equal(
      "news-portal",
      await classifySite("https://yahoo.com", RemoteSettings)
    );

    assert.equal(
      "social-media",
      await classifySite("http://twitter.com/firefox", RemoteSettings)
    );

    assert.equal(
      "ecommerce",
      await classifySite("https://amazon.com", RemoteSettings)
    );
    assert.equal(
      "ecommerce",
      await classifySite("https://amazon.ca", RemoteSettings)
    );
    assert.equal(
      "ecommerce",
      await classifySite("https://ebay.com", RemoteSettings)
    );
  });
});
