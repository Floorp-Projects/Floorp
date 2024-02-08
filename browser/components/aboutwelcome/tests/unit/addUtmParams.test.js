import { addUtmParams, BASE_PARAMS } from "content-src/lib/addUtmParams.mjs";

describe("addUtmParams", () => {
  const originalBaseParams = JSON.parse(JSON.stringify(BASE_PARAMS));
  afterEach(() => Object.assign(BASE_PARAMS, originalBaseParams));
  it("should convert a string URL", () => {
    const result = addUtmParams("https://foo.com", "foo");
    assert.equal(result.hostname, "foo.com");
  });
  it("should add all base params", () => {
    assert.match(
      addUtmParams(new URL("https://foo.com"), "foo").toString(),
      /utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral/
    );
  });
  it("should allow updating base params utm values", () => {
    BASE_PARAMS.utm_campaign = "firstrun-default";
    assert.match(
      addUtmParams(new URL("https://foo.com"), "foo", "default").toString(),
      /utm_source=activity-stream&utm_campaign=firstrun-default&utm_medium=referral/
    );
  });
  it("should add utm_term", () => {
    const params = addUtmParams(new URL("https://foo.com"), "foo").searchParams;
    assert.equal(params.get("utm_term"), "foo", "utm_term");
  });
  it("should not override the URL's existing utm param values", () => {
    const url = new URL("https://foo.com/?utm_source=foo&utm_campaign=bar");
    const params = addUtmParams(url, "foo").searchParams;
    assert.equal(params.get("utm_source"), "foo", "utm_source");
    assert.equal(params.get("utm_campaign"), "bar", "utm_campaign");
    assert.equal(params.get("utm_medium"), "referral", "utm_medium");
  });
});
