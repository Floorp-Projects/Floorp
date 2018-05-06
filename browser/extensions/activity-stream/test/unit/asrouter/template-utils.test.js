import {safeURI} from "content-src/asrouter/template-utils";

describe("safeURI", () => {
  let warnStub;
  beforeEach(() => {
    warnStub = sinon.stub(console, "warn");
  });
  afterEach(() => {
    warnStub.restore();
  });
  it("should allow http: URIs", () => {
    assert.equal(safeURI("http://foo.com"), "http://foo.com");
  });
  it("should allow https: URIs", () => {
    assert.equal(safeURI("https://foo.com"), "https://foo.com");
  });
  it("should allow data URIs", () => {
    assert.equal(safeURI("data:image/png;base64,iVBO"), "data:image/png;base64,iVBO");
  });
  it("should not allow javascript: URIs", () => {
    assert.equal(safeURI("javascript:foo()"), ""); // eslint-disable-line no-script-url
    assert.calledOnce(warnStub);
  });
  it("should not warn if the URL is falsey ", () => {
    assert.equal(safeURI(), "");
    assert.notCalled(warnStub);
  });
});
