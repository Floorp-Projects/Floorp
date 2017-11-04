const {shortURL} = require("lib/ShortURL.jsm");
const {GlobalOverrider} = require("test/unit/utils");

describe("shortURL", () => {
  let globals;
  let IDNStub;
  let getPublicSuffixStub;
  let newURIStub;

  beforeEach(() => {
    IDNStub = sinon.stub().callsFake(id => id);
    getPublicSuffixStub = sinon.stub().returns("com");
    newURIStub = sinon.stub().callsFake(id => id);

    globals = new GlobalOverrider();
    globals.set("IDNService", {convertToDisplayIDN: IDNStub});
    globals.set("Services", {
      eTLD: {getPublicSuffix: getPublicSuffixStub},
      io: {newURI: newURIStub}
    });
  });

  afterEach(() => {
    globals.restore();
  });

  it("should return a blank string if url is falsey", () => {
    assert.equal(shortURL({url: false}), "");
    assert.equal(shortURL({url: ""}), "");
    assert.equal(shortURL({}), "");
  });

  it("should remove the eTLD", () => {
    assert.equal(shortURL({url: "http://com.blah.com"}), "com.blah");
  });

  it("should call convertToDisplayIDN when calling shortURL", () => {
    const hostname = shortURL({url: "http://com.blah.com"});

    assert.calledOnce(IDNStub);
    assert.calledWithExactly(IDNStub, hostname, {});
  });

  it("should call getPublicSuffix", () => {
    shortURL({url: "http://bar.com"});

    assert.calledWithExactly(newURIStub, "http://bar.com");
    assert.calledOnce(newURIStub);
    assert.calledOnce(getPublicSuffixStub);
  });

  it("should get the hostname from .url", () => {
    assert.equal(shortURL({url: "http://bar.com"}), "bar");
  });

  it("should not strip out www if not first subdomain", () => {
    assert.equal(shortURL({url: "http://foo.www.com"}), "foo.www");
  });

  it("should convert to lowercase", () => {
    assert.equal(shortURL({url: "HTTP://FOO.COM"}), "foo");
  });

  it("should return hostname for localhost", () => {
    getPublicSuffixStub.throws("insufficient domain levels");

    assert.equal(shortURL({url: "http://localhost:8000/"}), "localhost");
  });

  it("should fallback to link title if it exists", () => {
    const link = {
      url: "file:///Users/voprea/Work/activity-stream/logs/coverage/system-addon/report-html/index.html",
      title: "Code coverage report"
    };

    assert.equal(shortURL(link), link.title);
  });

  it("should return the url if no title is provided", () => {
    const url = "file://foo/bar.txt";
    assert.equal(shortURL({url}), url);
  });
});
