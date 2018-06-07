import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {ComponentPerfTimer} from "content-src/components/ComponentPerfTimer/ComponentPerfTimer";
import createMockRaf from "mock-raf";
import React from "react";

import {shallow} from "enzyme";

const perfSvc = {
  mark() {},
  getMostRecentAbsMarkStartByName() {}
};

let DEFAULT_PROPS = {
  initialized: true,
  rows: [],
  id: "highlights",
  dispatch() {},
  intl: {formatMessage: x => x},
  perfSvc
};

describe("<ComponentPerfTimer>", () => {
  let mockRaf;
  let sandbox;
  let wrapper;

  const InnerEl = () => (<div>Inner Element</div>);

  beforeEach(() => {
    mockRaf = createMockRaf();
    sandbox = sinon.sandbox.create();
    sandbox.stub(window, "requestAnimationFrame").callsFake(mockRaf.raf);
    wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
  });
  afterEach(() => {
    sandbox.restore();
  });

  it("should render props.children", () => {
    assert.ok(wrapper.contains(<InnerEl />));
  });

  describe("#constructor", () => {
    beforeEach(() => {
      sandbox.stub(ComponentPerfTimer.prototype, "_maybeSendBadStateEvent");
      sandbox.stub(ComponentPerfTimer.prototype, "_ensureFirstRenderTsRecorded");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>, {disableLifecycleMethods: true});
    });

    it("should have the correct defaults", () => {
      const instance = wrapper.instance();

      assert.isFalse(instance._reportMissingData);
      assert.isFalse(instance._timestampHandled);
      assert.isFalse(instance._recordedFirstRender);
    });
  });

  describe("#render", () => {
    beforeEach(() => {
      sandbox.stub(DEFAULT_PROPS, "id").value("fake_section");
      sandbox.stub(ComponentPerfTimer.prototype, "_maybeSendBadStateEvent");
      sandbox.stub(ComponentPerfTimer.prototype, "_ensureFirstRenderTsRecorded");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
    });

    it("should not call telemetry on sections that we don't want to record", () => {
      const instance = wrapper.instance();

      assert.notCalled(instance._maybeSendBadStateEvent);
      assert.notCalled(instance._ensureFirstRenderTsRecorded);
    });
  });

  describe("#_componentDidMount", () => {
    it("should call _maybeSendPaintedEvent", () => {
      const instance = wrapper.instance();
      const stub = sandbox.stub(instance, "_maybeSendPaintedEvent");

      instance.componentDidMount();

      assert.calledOnce(stub);
    });

    it("should not call _maybeSendPaintedEvent if id not in RECORDED_SECTIONS", () => {
      sandbox.stub(DEFAULT_PROPS, "id").value("topstories");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();
      const stub = sandbox.stub(instance, "_maybeSendPaintedEvent");

      instance.componentDidMount();

      assert.notCalled(stub);
    });
  });

  describe("#_componentDidUpdate", () => {
    it("should call _maybeSendPaintedEvent", () => {
      const instance = wrapper.instance();
      const maybeSendPaintStub = sandbox.stub(instance, "_maybeSendPaintedEvent");

      instance.componentDidUpdate();

      assert.calledOnce(maybeSendPaintStub);
    });

    it("should not call _maybeSendPaintedEvent if id not in RECORDED_SECTIONS", () => {
      sandbox.stub(DEFAULT_PROPS, "id").value("topstories");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();
      const stub = sandbox.stub(instance, "_maybeSendPaintedEvent");

      instance.componentDidUpdate();

      assert.notCalled(stub);
    });
  });

  describe("_ensureFirstRenderTsRecorded", () => {
    let recordFirstRenderStub;
    beforeEach(() => {
      sandbox.stub(ComponentPerfTimer.prototype, "_maybeSendBadStateEvent");
      recordFirstRenderStub = sandbox.stub(ComponentPerfTimer.prototype, "_ensureFirstRenderTsRecorded");
    });

    it("should set _recordedFirstRender", () => {
      sandbox.stub(DEFAULT_PROPS, "initialized").value(false);
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();

      assert.isFalse(instance._recordedFirstRender);

      recordFirstRenderStub.callThrough();
      instance._ensureFirstRenderTsRecorded();

      assert.isTrue(instance._recordedFirstRender);
    });

    it("should mark first_render_ts", () => {
      sandbox.stub(DEFAULT_PROPS, "initialized").value(false);
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();
      const stub = sandbox.stub(perfSvc, "mark");

      recordFirstRenderStub.callThrough();
      instance._ensureFirstRenderTsRecorded();

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, `${DEFAULT_PROPS.id}_first_render_ts`);
    });
  });

  describe("#_maybeSendBadStateEvent", () => {
    let sendBadStateStub;
    beforeEach(() => {
      sendBadStateStub = sandbox.stub(ComponentPerfTimer.prototype, "_maybeSendBadStateEvent");
      sandbox.stub(ComponentPerfTimer.prototype, "_ensureFirstRenderTsRecorded");
    });

    it("should set this._reportMissingData=true when called with initialized === false", () => {
      sandbox.stub(DEFAULT_PROPS, "initialized").value(false);
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();

      assert.isFalse(instance._reportMissingData);

      sendBadStateStub.callThrough();
      instance._maybeSendBadStateEvent();

      assert.isTrue(instance._reportMissingData);
    });

    it("should call _sendBadStateEvent if initialized & other metrics have been recorded", () => {
      const instance = wrapper.instance();
      const stub = sandbox.stub(instance, "_sendBadStateEvent");
      instance._reportMissingData = true;
      instance._timestampHandled = true;
      instance._recordedFirstRender = true;

      sendBadStateStub.callThrough();
      instance._maybeSendBadStateEvent();

      assert.calledOnce(stub);
      assert.isFalse(instance._reportMissingData);
    });
  });

  describe("#_maybeSendPaintedEvent", () => {
    it("should call _sendPaintedEvent if props.initialized is true", () => {
      sandbox.stub(DEFAULT_PROPS, "initialized").value(true);
      wrapper = shallow(
        <ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>,
        {disableLifecycleMethods: true});
      const instance = wrapper.instance();
      const stub = sandbox.stub(instance, "_afterFramePaint");

      assert.isFalse(instance._timestampHandled);

      instance._maybeSendPaintedEvent();

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, instance._sendPaintedEvent);
      assert.isTrue(wrapper.instance()._timestampHandled);
    });
    it("should not call _sendPaintedEvent if this._timestampHandled is true", () => {
      const instance = wrapper.instance();
      const spy = sinon.spy(instance, "_afterFramePaint");
      instance._timestampHandled = true;

      instance._maybeSendPaintedEvent();
      spy.neverCalledWith(instance._sendPaintedEvent);
    });
    it("should not call _sendPaintedEvent if component not initialized", () => {
      sandbox.stub(DEFAULT_PROPS, "initialized").value(false);
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);
      const instance = wrapper.instance();
      const spy = sinon.spy(instance, "_afterFramePaint");

      instance._maybeSendPaintedEvent();

      spy.neverCalledWith(instance._sendPaintedEvent);
    });
  });

  describe("#_afterFramePaint", () => {
    it("should call callback after the requestAnimationFrame callback returns", done => {
      // Setting the callback to done is the test that it does finally get
      // called at the correct time, after the event loop ticks again.
      // If it doesn't get called, this test will time out.
      this.callback = () => done();
      sandbox.spy(this, "callback");

      const instance = wrapper.instance();

      instance._afterFramePaint(this.callback);

      assert.notCalled(this.callback);
      mockRaf.step({count: 1});
    });
  });

  describe("#_sendBadStateEvent", () => {
    it("should call perfSvc.mark", () => {
      sandbox.spy(perfSvc, "mark");
      const key = `${DEFAULT_PROPS.id}_data_ready_ts`;

      wrapper.instance()._sendBadStateEvent();

      assert.calledOnce(perfSvc.mark);
      assert.calledWithExactly(perfSvc.mark, key);
    });

    it("should call compute the delta from first render to data ready", () => {
      sandbox.stub(perfSvc, "getMostRecentAbsMarkStartByName");

      wrapper.instance()._sendBadStateEvent(`${DEFAULT_PROPS.id}_data_ready_ts`);

      assert.calledTwice(perfSvc.getMostRecentAbsMarkStartByName);
      assert.calledWithExactly(perfSvc.getMostRecentAbsMarkStartByName, `${DEFAULT_PROPS.id}_data_ready_ts`);
      assert.calledWithExactly(perfSvc.getMostRecentAbsMarkStartByName, `${DEFAULT_PROPS.id}_first_render_ts`);
    });

    it("should call dispatch SAVE_SESSION_PERF_DATA", () => {
      sandbox.stub(perfSvc, "getMostRecentAbsMarkStartByName")
        .withArgs("highlights_first_render_ts").returns(0.5)
        .withArgs("highlights_data_ready_ts")
        .returns(3.2);

      const dispatch = sandbox.spy(DEFAULT_PROPS, "dispatch");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendBadStateEvent();

      assert.calledOnce(dispatch);
      assert.calledWithExactly(dispatch, ac.OnlyToMain({
        type: at.SAVE_SESSION_PERF_DATA,
        data: {[`${DEFAULT_PROPS.id}_data_late_by_ms`]: 2}
      }));
    });
  });

  describe("#_sendPaintedEvent", () => {
    beforeEach(() => {
      sandbox.stub(ComponentPerfTimer.prototype, "_maybeSendBadStateEvent");
      sandbox.stub(ComponentPerfTimer.prototype, "_ensureFirstRenderTsRecorded");
    });

    it("should not call mark with the wrong id", () => {
      sandbox.stub(perfSvc, "mark");
      sandbox.stub(DEFAULT_PROPS, "id").value("fake_id");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendPaintedEvent();

      assert.notCalled(perfSvc.mark);
    });
    it("should call mark with the correct topsites", () => {
      sandbox.stub(perfSvc, "mark");
      sandbox.stub(DEFAULT_PROPS, "id").value("topsites");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendPaintedEvent();

      assert.calledOnce(perfSvc.mark);
      assert.calledWithExactly(perfSvc.mark, "topsites_first_painted_ts");
    });
    it("should not call getMostRecentAbsMarkStartByName if id!=topsites", () => {
      sandbox.stub(perfSvc, "getMostRecentAbsMarkStartByName");
      sandbox.stub(DEFAULT_PROPS, "id").value("fake_id");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendPaintedEvent();

      assert.notCalled(perfSvc.getMostRecentAbsMarkStartByName);
    });
    it("should call getMostRecentAbsMarkStartByName for topsites", () => {
      sandbox.stub(perfSvc, "getMostRecentAbsMarkStartByName");
      sandbox.stub(DEFAULT_PROPS, "id").value("topsites");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendPaintedEvent();

      assert.calledOnce(perfSvc.getMostRecentAbsMarkStartByName);
      assert.calledWithExactly(perfSvc.getMostRecentAbsMarkStartByName,
                               "topsites_first_painted_ts");
    });
    it("should dispatch SAVE_SESSION_PERF_DATA", () => {
      sandbox.stub(perfSvc, "getMostRecentAbsMarkStartByName").returns(42);
      sandbox.stub(DEFAULT_PROPS, "id").value("topsites");
      const dispatch = sandbox.spy(DEFAULT_PROPS, "dispatch");
      wrapper = shallow(<ComponentPerfTimer {...DEFAULT_PROPS}><InnerEl /></ComponentPerfTimer>);

      wrapper.instance()._sendPaintedEvent();

      assert.calledOnce(dispatch);
      assert.calledWithExactly(dispatch, ac.OnlyToMain({
        type: at.SAVE_SESSION_PERF_DATA,
        data: {"topsites_first_painted_ts": 42}
      }));
    });
  });
});
