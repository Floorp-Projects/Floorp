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
    const header = wrapper.find(
      "h2[data-l10n-id='newtab-discovery-empty-section-topstories-header']"
    );
    const paragraph = wrapper.find(
      "p[data-l10n-id='newtab-discovery-empty-section-topstories-content']"
    );

    assert.ok(header.exists());
    assert.ok(paragraph.exists());
  });

  it("should render failed state message", () => {
    wrapper = shallow(<DSEmptyState status="failed" />);
    const button = wrapper.find(
      "button[data-l10n-id='newtab-discovery-empty-section-topstories-try-again-button']"
    );

    assert.ok(button.exists());
  });

  it("should render waiting state message", () => {
    wrapper = shallow(<DSEmptyState status="waiting" />);
    const button = wrapper.find(
      "button[data-l10n-id='newtab-discovery-empty-section-topstories-loading']"
    );

    assert.ok(button.exists());
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
