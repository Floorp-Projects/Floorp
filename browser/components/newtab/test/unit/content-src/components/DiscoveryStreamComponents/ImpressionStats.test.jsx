"use strict";

import {ImpressionStats, INTERSECTION_RATIO} from "content-src/components/DiscoveryStreamImpressionStats/ImpressionStats";
import {actionTypes as at} from "common/Actions.jsm";
import React from "react";
import {shallow} from "enzyme";

describe("<ImpressionStats>", () => {
  const SOURCE = "TEST_SOURCE";
  const FullIntersectEntries = [{isIntersecting: true, intersectionRatio: INTERSECTION_RATIO}];
  const ZeroIntersectEntries = [{isIntersecting: false, intersectionRatio: 0}];
  const PartialIntersectEntries = [{isIntersecting: true, intersectionRatio: INTERSECTION_RATIO / 2}];

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
    rows: [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}],
    source: SOURCE,
    IntersectionObserver: buildIntersectionObserver(FullIntersectEntries),
    document: {
      visibilityState: "visible",
      addEventListener: sinon.stub(),
      removeEventListener: sinon.stub(),
    },
  };

  const InnerEl = () => (<div>Inner Element</div>);

  function renderImpressionStats(props = {}) {
    return shallow(<ImpressionStats {...DEFAULT_PROPS} {...props}>
        <InnerEl />
      </ImpressionStats>);
  }

  it("should render props.children", () => {
    const wrapper = renderImpressionStats();
    assert.ok(wrapper.contains(<InnerEl />));
  });
  it("should not send loaded content nor impression when the page is not visible", () => {
    const dispatch = sinon.spy();
    const props = {
      dispatch,
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
    };
    renderImpressionStats(props);

    assert.notCalled(dispatch);
  });
  it("should noly send loaded content but not impression when the wrapped item is not visbible", () => {
    const dispatch = sinon.spy();
    const props = {dispatch, IntersectionObserver: buildIntersectionObserver(ZeroIntersectEntries)};
    renderImpressionStats(props);

    // This one is for loaded content.
    assert.calledOnce(dispatch);
    const [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_LOADED_CONTENT);
    assert.equal(action.data.source, SOURCE);
    assert.deepEqual(action.data.tiles,
      [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]);
  });
  it("should not send impression when the wrapped item is visbible but below the ratio", () => {
    const dispatch = sinon.spy();
    const props = {dispatch, IntersectionObserver: buildIntersectionObserver(PartialIntersectEntries)};
    renderImpressionStats(props);

    // This one is for loaded content.
    assert.calledOnce(dispatch);
  });
  it("should send a loaded content and an impression when the page is visible and the wrapped item meets the visibility ratio", () => {
    const dispatch = sinon.spy();
    const props = {dispatch, IntersectionObserver: buildIntersectionObserver(FullIntersectEntries)};
    renderImpressionStats(props);

    assert.calledTwice(dispatch);

    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_LOADED_CONTENT);
    assert.equal(action.data.source, SOURCE);
    assert.deepEqual(action.data.tiles,
      [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]);

    [action] = dispatch.secondCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_IMPRESSION_STATS);
    assert.equal(action.data.source, SOURCE);
    assert.deepEqual(action.data.tiles,
      [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]);
  });
  it("should send a DISCOVERY_STREAM_SPOC_IMPRESSION when the wrapped item has a campaignId", () => {
    const dispatch = sinon.spy();
    const campaignId = "a_campaign_id";
    const props = {dispatch, campaignId, IntersectionObserver: buildIntersectionObserver(FullIntersectEntries)};
    renderImpressionStats(props);

    // Loaded content + DISCOVERY_STREAM_SPOC_IMPRESSION + impression
    assert.calledThrice(dispatch);

    const [action] = dispatch.secondCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_SPOC_IMPRESSION);
    assert.deepEqual(action.data, {campaignId});
  });
  it("should send an impression when the wrapped item transiting from invisible to visible", () => {
    const dispatch = sinon.spy();
    const props = {dispatch, IntersectionObserver: buildIntersectionObserver(ZeroIntersectEntries, false)};
    const wrapper = renderImpressionStats(props);

    // For the loaded content
    assert.calledOnce(dispatch);

    let [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_LOADED_CONTENT);
    assert.equal(action.data.source, SOURCE);
    assert.deepEqual(action.data.tiles,
      [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]);

    dispatch.resetHistory();

    // Simulating the full intersection change with a row change
    wrapper.setProps({
      ...props,
      ...{rows: [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]},
      ...{IntersectionObserver: buildIntersectionObserver(FullIntersectEntries)},
    });

    // For the impression
    assert.calledOnce(dispatch);

    [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.DISCOVERY_STREAM_IMPRESSION_STATS);
    assert.deepEqual(action.data.tiles,
      [{id: 1, pos: 0}, {id: 2, pos: 1}, {id: 3, pos: 2}]);
  });
  it("should send a loaded content and an impression if props are updated and props.rows are different", () => {
    const props = {dispatch: sinon.spy()};
    const wrapper = renderImpressionStats(props);
    props.dispatch.resetHistory();

    // New rows
    wrapper.setProps({...DEFAULT_PROPS, ...{rows: [{id: 4, pos: 3}]}});

    assert.calledTwice(props.dispatch);
  });
  it("should not send any ping if props are updated but IDs are the same", () => {
    const props = {dispatch: sinon.spy()};
    const wrapper = renderImpressionStats(props);
    props.dispatch.resetHistory();

    wrapper.setProps(DEFAULT_PROPS);

    assert.notCalled(props.dispatch);
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

    const wrapper = renderImpressionStats(props);
    assert.calledWith(props.document.addEventListener, "visibilitychange");
    const [, listener] = props.document.addEventListener.firstCall.args;

    wrapper.unmount();
    assert.calledWith(props.document.removeEventListener, "visibilitychange", listener);
  });
  it("should unobserve the intersection observer when the wrapper is removed", () => {
    const IntersectionObserver = buildIntersectionObserver(ZeroIntersectEntries);
    const spy = sinon.spy(IntersectionObserver.prototype, "unobserve");
    const props = {dispatch: sinon.spy(), IntersectionObserver};

    const wrapper = renderImpressionStats(props);
    wrapper.unmount();

    assert.calledOnce(spy);
  });
  it("should only send the latest impression on a visibility change", () => {
    const listeners = new Set();
    const props = {
      dispatch: sinon.spy(),
      document: {
        visibilityState: "hidden",
        addEventListener: (ev, cb) => listeners.add(cb),
        removeEventListener: (ev, cb) => listeners.delete(cb),
      },
    };

    const wrapper = renderImpressionStats(props);

    // Update twice
    wrapper.setProps({...props, ...{rows: [{id: 123, pos: 4}]}});
    wrapper.setProps({...props, ...{rows: [{id: 2432, pos: 5}]}});

    assert.notCalled(props.dispatch);

    // Simulate listeners getting called
    props.document.visibilityState = "visible";
    listeners.forEach(l => l());

    // Make sure we only sent the latest event
    assert.calledTwice(props.dispatch);
    const [action] = props.dispatch.firstCall.args;
    assert.deepEqual(action.data.tiles, [{id: 2432, pos: 5}]);
  });
});
