import { SYSTEM_TICK_INTERVAL, SystemTickFeed } from "lib/SystemTickFeed.jsm";
import { actionTypes as at } from "common/Actions.sys.mjs";
import { GlobalOverrider } from "test/unit/utils";

describe("System Tick Feed", () => {
  let globals;
  let instance;
  let clock;

  beforeEach(() => {
    globals = new GlobalOverrider();
    clock = sinon.useFakeTimers();

    instance = new SystemTickFeed();
    instance.store = {
      getState() {
        return {};
      },
      dispatch() {},
    };
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });
  it("should create a SystemTickFeed", () => {
    assert.instanceOf(instance, SystemTickFeed);
  });
  it("should fire SYSTEM_TICK events at configured interval", () => {
    globals.set("ChromeUtils", {
      idleDispatch: f => f(),
    });
    let expectation = sinon
      .mock(instance.store)
      .expects("dispatch")
      .twice()
      .withExactArgs({ type: at.SYSTEM_TICK });

    instance.onAction({ type: at.INIT });
    clock.tick(SYSTEM_TICK_INTERVAL * 2);
    expectation.verify();
  });
  it("should not fire SYSTEM_TICK events after UNINIT", () => {
    let expectation = sinon.mock(instance.store).expects("dispatch").never();

    instance.onAction({ type: at.UNINIT });
    clock.tick(SYSTEM_TICK_INTERVAL * 2);
    expectation.verify();
  });
  it("should not fire SYSTEM_TICK events while the user is away", () => {
    let expectation = sinon.mock(instance.store).expects("dispatch").never();

    instance.onAction({ type: at.INIT });
    instance._idleService = { idleTime: SYSTEM_TICK_INTERVAL + 1 };
    clock.tick(SYSTEM_TICK_INTERVAL * 3);
    expectation.verify();
    instance.onAction({ type: at.UNINIT });
  });
  it("should fire SYSTEM_TICK immediately when the user is active again", () => {
    globals.set("ChromeUtils", {
      idleDispatch: f => f(),
    });
    let expectation = sinon
      .mock(instance.store)
      .expects("dispatch")
      .once()
      .withExactArgs({ type: at.SYSTEM_TICK });

    instance.onAction({ type: at.INIT });
    instance._idleService = { idleTime: SYSTEM_TICK_INTERVAL + 1 };
    clock.tick(SYSTEM_TICK_INTERVAL * 3);
    instance.observe();
    expectation.verify();
    instance.onAction({ type: at.UNINIT });
  });
});
