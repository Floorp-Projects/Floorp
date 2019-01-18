import {actionTypes as at} from "common/Actions.jsm";
import {ImpressionStats} from "content-src/components/DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import {shallow} from "enzyme";

const SOURCE = "TEST_SOURCE";

describe("<ImpressionStats>", () => {
  const DEFAULT_PROPS = {
    rows: [{id: 1}, {id: 2}, {id: 3}],
    source: SOURCE,
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
  it("should send impression with the right stats when the page loads", () => {
    const dispatch = sinon.spy();
    const props = {dispatch};
    renderImpressionStats(props);

    assert.calledOnce(dispatch);

    const [action] = dispatch.firstCall.args;
    assert.equal(action.type, at.TELEMETRY_IMPRESSION_STATS);
    assert.equal(action.data.source, SOURCE);
    assert.deepEqual(action.data.tiles, [{id: 1}, {id: 2}, {id: 3}]);
  });
  it("should send 1 impression when the page becomes visibile after loading", () => {
    const props = {
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
      dispatch: sinon.spy(),
    };

    renderImpressionStats(props);

    // Was the event listener added?
    assert.calledWith(props.document.addEventListener, "visibilitychange");

    // Make sure dispatch wasn't called yet
    assert.notCalled(props.dispatch);

    // Simulate a visibilityChange event
    const [, listener] = props.document.addEventListener.firstCall.args;
    props.document.visibilityState = "visible";
    listener();

    // Did we actually dispatch an event?
    assert.calledOnce(props.dispatch);
    assert.equal(props.dispatch.firstCall.args[0].type, at.TELEMETRY_IMPRESSION_STATS);

    // Did we remove the event listener?
    assert.calledWith(props.document.removeEventListener, "visibilitychange", listener);
  });
  it("should remove visibility change listener when wrapper is removed", () => {
    const props = {
      dispatch: sinon.spy(),
      document: {
        visibilityState: "hidden",
        addEventListener: sinon.spy(),
        removeEventListener: sinon.spy(),
      },
    };

    const wrapper = renderImpressionStats(props);
    assert.calledWith(props.document.addEventListener, "visibilitychange");
    const [, listener] = props.document.addEventListener.firstCall.args;

    wrapper.unmount();
    assert.calledWith(props.document.removeEventListener, "visibilitychange", listener);
  });
  it("should send an impression if props are updated and props.rows are different", () => {
    const props = {dispatch: sinon.spy()};
    const wrapper = renderImpressionStats(props);
    props.dispatch.resetHistory();

    // New rows
    wrapper.setProps({...DEFAULT_PROPS, ...{rows: [{id: 4}]}});

    assert.calledOnce(props.dispatch);
  });
  it("should not send an impression if props are updated but IDs are the same", () => {
    const props = {dispatch: sinon.spy()};
    const wrapper = renderImpressionStats(props);
    props.dispatch.resetHistory();

    wrapper.setProps(DEFAULT_PROPS);

    assert.notCalled(props.dispatch);
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
    wrapper.setProps({...props, ...{rows: [{id: 123}]}});
    wrapper.setProps({...props, ...{rows: [{id: 2432}]}});

    assert.notCalled(props.dispatch);

    // Simulate listeners getting called
    props.document.visibilityState = "visible";
    listeners.forEach(l => l());

    // Make sure we only sent the latest event
    assert.calledOnce(props.dispatch);
    const [action] = props.dispatch.firstCall.args;
    assert.deepEqual(action.data.tiles, [{id: 2432}]);
  });
});
