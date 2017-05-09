const shortURL = require("content-src/lib/short-url");

describe("shortURL", () => {
  it("should return a blank string if url and hostname is falsey", () => {
    assert.equal(shortURL({url: ""}), "");
    assert.equal(shortURL({hostname: null}), "");
  });

  it("should remove the eTLD, if provided", () => {
    assert.equal(shortURL({hostname: "com.blah.com", eTLD: "com"}), "com.blah");
  });

  it("should use the hostname, if provided", () => {
    assert.equal(shortURL({hostname: "foo.com", url: "http://bar.com", eTLD: "com"}), "foo");
  });

  it("should get the hostname from .url if necessary", () => {
    assert.equal(shortURL({url: "http://bar.com", eTLD: "com"}), "bar");
  });

  it("should not strip out www if not first subdomain", () => {
    assert.equal(shortURL({hostname: "foo.www.com", eTLD: "com"}), "foo.www");
  });

  it("should convert to lowercase", () => {
    assert.equal(shortURL({url: "HTTP://FOO.COM", eTLD: "com"}), "foo");
  });
});
