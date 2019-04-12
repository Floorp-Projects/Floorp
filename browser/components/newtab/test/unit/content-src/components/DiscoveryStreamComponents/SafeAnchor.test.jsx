import React from "react";
import {SafeAnchor} from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import {shallow} from "enzyme";

describe("Discovery Stream <SafeAnchor>", () => {
  let warnStub;
  let sandbox;
  beforeEach(() => {
    warnStub = sinon.stub(console, "warn");
    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    warnStub.restore();
    sandbox.restore();
  });
  it("should render with anchor", () => {
    const wrapper = shallow(<SafeAnchor />);
    assert.lengthOf(wrapper.find("a"), 1);
  });
  it("should render with anchor target for http", () => {
    const wrapper = shallow(<SafeAnchor url="http://example.com" />);
    assert.equal(wrapper.find("a").prop("href"), "http://example.com");
  });
  it("should render with anchor target for https", () => {
    const wrapper = shallow(<SafeAnchor url="https://example.com" />);
    assert.equal(wrapper.find("a").prop("href"), "https://example.com");
  });
  it("should not allow javascript: URIs", () => {
    const wrapper = shallow(<SafeAnchor url="javascript:foo()" />); // eslint-disable-line no-script-url
    assert.equal(wrapper.find("a").prop("href"), "");
    assert.calledOnce(warnStub);
  });
  it("should not warn if the URL is falsey ", () => {
    const wrapper = shallow(<SafeAnchor url="" />);
    assert.equal(wrapper.find("a").prop("href"), "");
    assert.notCalled(warnStub);
  });
  it("should dispatch an event on click", () => {
    const dispatchStub = sandbox.stub();
    const fakeEvent = {preventDefault: sandbox.stub(), currentTarget: {}};
    const wrapper = shallow(<SafeAnchor dispatch={dispatchStub} />);

    wrapper.find("a").simulate("click", fakeEvent);

    assert.calledOnce(dispatchStub);
    assert.calledOnce(fakeEvent.preventDefault);
  });
  it("should call onLinkClick if provided", () => {
    const onLinkClickStub = sandbox.stub();
    const wrapper = shallow(<SafeAnchor onLinkClick={onLinkClickStub} />);

    wrapper.find("a").simulate("click");

    assert.calledOnce(onLinkClickStub);
  });
});
