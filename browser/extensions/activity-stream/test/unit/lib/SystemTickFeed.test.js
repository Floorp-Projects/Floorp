"use strict";
const injector = require("inject!lib/SystemTickFeed.jsm");
const {actionTypes: at} = require("common/Actions.jsm");

describe("System Tick Feed", () => {
  let SystemTickFeed;
  let SYSTEM_TICK_INTERVAL;
  let instance;
  let clock;

  beforeEach(() => {
    clock = sinon.useFakeTimers();

    ({SystemTickFeed, SYSTEM_TICK_INTERVAL} = injector({}));
    instance = new SystemTickFeed();
    instance.store = {getState() { return {}; }, dispatch() {}};
  });
  afterEach(() => {
    clock.restore();
  });
  it("should create a SystemTickFeed", () => {
    assert.instanceOf(instance, SystemTickFeed);
  });
  it("should fire SYSTEM_TICK events at configured interval", () => {
    let expectation = sinon.mock(instance.store).expects("dispatch")
      .twice()
      .withExactArgs({type: at.SYSTEM_TICK});

    instance.onAction({type: at.INIT});
    clock.tick(SYSTEM_TICK_INTERVAL * 2);
    expectation.verify();
  });
  it("should not fire SYSTEM_TICK events after UNINIT", () => {
    let expectation = sinon.mock(instance.store).expects("dispatch")
      .never();

    instance.onAction({type: at.UNINIT});
    clock.tick(SYSTEM_TICK_INTERVAL * 2);
    expectation.verify();
  });
});
