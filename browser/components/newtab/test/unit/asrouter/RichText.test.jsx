import {convertLinks} from "content-src/asrouter/components/RichText/RichText";

describe("convertLinks", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should return an object with anchor elements", () => {
    const cta = {
      url: "https://foo.com",
      metric: "foo",
    };
    const stub = sandbox.stub();
    const result = convertLinks({cta}, stub);

    assert.property(result, "cta");
    assert.propertyVal(result.cta, "type", "a");
    assert.propertyVal(result.cta.props, "href", cta.url);
    assert.propertyVal(result.cta.props, "data-metric", cta.metric);
    assert.propertyVal(result.cta.props, "onClick", stub);
  });
  it("should return an anchor element without href", () => {
    const cta = {
      url: "https://foo.com",
      metric: "foo",
      action: "OPEN_MENU",
      args: "appMenu",
    };
    const stub = sandbox.stub();
    const result = convertLinks({cta}, stub);

    assert.property(result, "cta");
    assert.propertyVal(result.cta, "type", "a");
    assert.propertyVal(result.cta.props, "href", false);
    assert.propertyVal(result.cta.props, "data-metric", cta.metric);
    assert.propertyVal(result.cta.props, "data-action", cta.action);
    assert.propertyVal(result.cta.props, "data-args", cta.args);
    assert.propertyVal(result.cta.props, "onClick", stub);
  });
});
