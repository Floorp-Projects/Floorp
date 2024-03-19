import {
  TopSiteImpressionWrapper,
  INTERSECTION_RATIO,
} from "content-src/components/TopSites/TopSiteImpressionWrapper";
import { actionTypes as at } from "common/Actions.mjs";
import React from "react";
import { shallow } from "enzyme";

describe("<TopSiteImpressionWrapper>", () => {
  const FullIntersectEntries = [
    { isIntersecting: true, intersectionRatio: INTERSECTION_RATIO },
  ];
  const ZeroIntersectEntries = [
    { isIntersecting: false, intersectionRatio: 0 },
  ];
  const PartialIntersectEntries = [
    { isIntersecting: true, intersectionRatio: INTERSECTION_RATIO / 2 },
  ];

  // Build IntersectionObserver class with the arg `entries` for the intersect callback.
  function buildIntersectionObserver(entries) {
    return class {
      constructor(callback) {
        this.callback = callback;
      }

      observe() {
        this.callback(entries);
      }

      unobserve() {}
    };
  }

  const DEFAULT_PROPS = {
    actionType: at.TOP_SITES_SPONSORED_IMPRESSION_STATS,
    tile: {
      tile_id: 1,
      position: 1,
      reporting_url: "https://test.reporting.com",
      advertiser: "test_advertiser",
    },
    IntersectionObserver: buildIntersectionObserver(FullIntersectEntries),
    document: {
      visibilityState: "visible",
      addEventListener: sinon.stub(),
      removeEventListener: sinon.stub(),
    },
  };

  const InnerEl = () => <div>Inner Element</div>;

  function renderTopSiteImpressionWrapper(props = {}) {
    return shallow(
      <TopSiteImpressionWrapper {...DEFAULT_PROPS} {...props}>
        <InnerEl />
      </TopSiteImpressionWrapper>
    );
  }

  it("should render props.children", () => {
    const wrapper = renderTopSiteImpressionWrapper();
    assert.ok(wrapper.contains(<InnerEl />));
  });
  it("should not send impression when the wrapped item is visbible but below the ratio", () => {
    const dispatch = sinon.spy();
    const props = {
      dispatch,
      IntersectionObserver: buildIntersectionObserver(PartialIntersectEntries),
    };
    renderTopSiteImpressionWrapper(props);

    assert.notCalled(dispatch);
  });
  it("should send an impression when the page is visible and the wrapped item meets the visibility ratio", () => {
    const dispatch = sinon.spy();
    const props = {
      dispatch,
      IntersectionObserver: buildIntersectionObserver(FullIntersectEntries),
    };
    renderTopSiteImpressionWrapper(props);

    assert.calledOnce(dispatch);

    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.TOP_SITES_SPONSORED_IMPRESSION_STATS);
    assert.deepEqual(action.data, {
      type: "impression",
      ...DEFAULT_PROPS.tile,
    });
  });
  it("should send an impression when the wrapped item transiting from invisible to visible", () => {
    const dispatch = sinon.spy();
    const props = {
      dispatch,
      IntersectionObserver: buildIntersectionObserver(ZeroIntersectEntries),
    };
    const wrapper = renderTopSiteImpressionWrapper(props);

    assert.notCalled(dispatch);

    dispatch.resetHistory();
    wrapper.instance().impressionObserver.callback(FullIntersectEntries);

    // For the impression
    assert.calledOnce(dispatch);

    const [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.TOP_SITES_SPONSORED_IMPRESSION_STATS);
    assert.deepEqual(action.data, {
      type: "impression",
      ...DEFAULT_PROPS.tile,
    });
  });
  it("should remove visibility change listener when the wrapper is removed", () => {
    const props = {
      dispatch: sinon.spy(),
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
      IntersectionObserver,
    };

    const wrapper = renderTopSiteImpressionWrapper(props);
    assert.calledWith(props.document.addEventListener, "visibilitychange");
    const [, listener] = props.document.addEventListener.firstCall.args;

    wrapper.unmount();
    assert.calledWith(
      props.document.removeEventListener,
      "visibilitychange",
      listener
    );
  });
  it("should unobserve the intersection observer when the wrapper is removed", () => {
    const IntersectionObserver =
      buildIntersectionObserver(ZeroIntersectEntries);
    const spy = sinon.spy(IntersectionObserver.prototype, "unobserve");
    const props = { dispatch: sinon.spy(), IntersectionObserver };

    const wrapper = renderTopSiteImpressionWrapper(props);
    wrapper.unmount();

    assert.calledOnce(spy);
  });
});
