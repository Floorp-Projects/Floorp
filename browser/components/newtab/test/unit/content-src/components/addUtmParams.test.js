import { addUtmParams } from "content-src/asrouter/templates/FirstRun/addUtmParams";

describe("addUtmParams", () => {
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
  it("should add utm_term", () => {
    const params = addUtmParams(new URL("https://foo.com"), "foo").searchParams;
    assert.equal(params.get("utm_term"), "foo", "utm_term");
  });
});
