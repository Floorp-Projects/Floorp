import { DSEmptyState } from "content-src/components/DiscoveryStreamComponents/DSEmptyState/DSEmptyState";
import React from "react";
import { shallow } from "enzyme";

describe("<DSEmptyState>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<DSEmptyState />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".section-empty-state").exists());
  });

  it("should render defaultempty state message", () => {
    assert.ok(wrapper.find(".empty-state-message").exists());
    assert.ok(wrapper.find("h2").exists());
    assert.ok(wrapper.find("p").exists());
  });

  it("should render failed state message", () => {
    wrapper = shallow(<DSEmptyState status="failed" />);
    assert.ok(wrapper.find("button.try-again-button").exists());
  });

  it("should render waiting state message", () => {
    wrapper = shallow(<DSEmptyState status="waiting" />);
    assert.ok(wrapper.find("button.try-again-button.waiting").exists());
  });

  it("should dispatch DISCOVERY_STREAM_RETRY_FEED on failed state button click", () => {
    const dispatch = sinon.spy();

    wrapper = shallow(
      <DSEmptyState
        status="failed"
        dispatch={dispatch}
        feed={{ url: "https://foo.com", data: {} }}
      />
    );
    wrapper.find("button.try-again-button").simulate("click");

    assert.calledTwice(dispatch);
    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, "DISCOVERY_STREAM_FEED_UPDATE");
    assert.deepEqual(action.data.feed, {
      url: "https://foo.com",
      data: { status: "waiting" },
    });

    [action] = dispatch.secondCall.args;

    assert.equal(action.type, "DISCOVERY_STREAM_RETRY_FEED");
    assert.deepEqual(action.data.feed, { url: "https://foo.com", data: {} });
  });
});
