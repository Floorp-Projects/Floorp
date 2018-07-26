import {GlobalOverrider} from "test/unit/utils";
import {shortURL} from "lib/ShortURL.jsm";

const puny = "xn--kpry57d";
const idn = "台灣";

describe("shortURL", () => {
  let globals;
  let IDNStub;
  let getPublicSuffixFromHostStub;

  beforeEach(() => {
    IDNStub = sinon.stub().callsFake(host => host.replace(puny, idn));
    getPublicSuffixFromHostStub = sinon.stub().returns("com");

    globals = new GlobalOverrider();
    globals.set("IDNService", {convertToDisplayIDN: IDNStub});
    globals.set("Services", {eTLD: {getPublicSuffixFromHost: getPublicSuffixFromHostStub}});
  });

  afterEach(() => {
    globals.restore();
  });

  it("should return a blank string if url is falsey", () => {
    assert.equal(shortURL({url: false}), "");
    assert.equal(shortURL({url: ""}), "");
    assert.equal(shortURL({}), "");
  });

  it("should return the 'url' if not a valid url", () => {
    const checkInvalid = url => assert.equal(shortURL({url}), url);
    checkInvalid(true);
    checkInvalid("something");
    checkInvalid("http:");
    checkInvalid("http::double");
    checkInvalid("http://badport:65536/");
  });

  it("should remove the eTLD", () => {
    assert.equal(shortURL({url: "http://com.blah.com"}), "com.blah");
  });

  it("should convert host to idn when calling shortURL", () => {
    assert.equal(shortURL({url: `http://${puny}.blah.com`}), `${idn}.blah`);
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

  it("should not include the port", () => {
    assert.equal(shortURL({url: "http://foo.com:8888"}), "foo");
  });

  it("should return hostname for localhost", () => {
    getPublicSuffixFromHostStub.throws("insufficient domain levels");

    assert.equal(shortURL({url: "http://localhost:8000/"}), "localhost");
  });

  it("should return hostname for ip address", () => {
    getPublicSuffixFromHostStub.throws("host is ip address");

    assert.equal(shortURL({url: "http://127.0.0.1/foo"}), "127.0.0.1");
  });

  it("should return etld for www.gov.uk (www-only non-etld)", () => {
    getPublicSuffixFromHostStub.returns("gov.uk");

    assert.equal(shortURL({url: "https://www.gov.uk/countersigning"}), "gov.uk");
  });

  it("should return idn etld for www-only non-etld", () => {
    getPublicSuffixFromHostStub.returns(puny);

    assert.equal(shortURL({url: `https://www.${puny}/foo`}), idn);
  });

  it("should return not the protocol for file:", () => {
    assert.equal(shortURL({url: "file:///foo/bar.txt"}), "/foo/bar.txt");
  });

  it("should return not the protocol for about:", () => {
    assert.equal(shortURL({url: "about:newtab"}), "newtab");
  });

  it("should fall back to full url as a last resort", () => {
    assert.equal(shortURL({url: "about:"}), "about:");
  });
});
