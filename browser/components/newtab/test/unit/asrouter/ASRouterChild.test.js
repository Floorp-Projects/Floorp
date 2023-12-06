/*eslint max-nested-callbacks: ["error", 10]*/
import { ASRouterChild } from "actors/ASRouterChild.sys.mjs";
import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.sys.mjs";
import { GlobalOverrider } from "test/unit/utils";

describe("ASRouterChild", () => {
  let asRouterChild = null;
  let globals = null;
  let overrider = null;
  let sandbox = null;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    globals = {
      Cu: {
        cloneInto: sandbox.stub().returns(Promise.resolve()),
      },
    };
    overrider = new GlobalOverrider();
    overrider.set(globals);
    asRouterChild = new ASRouterChild();
    asRouterChild.telemetry = {
      sendTelemetry: sandbox.stub(),
    };
    sandbox.stub(asRouterChild, "sendAsyncMessage");
    sandbox.stub(asRouterChild, "sendQuery").returns(Promise.resolve());
  });
  afterEach(() => {
    sandbox.restore();
    overrider.restore();
    asRouterChild = null;
  });
  describe("asRouterMessage", () => {
    describe("uses sendAsyncMessage for types that don't need an async response", () => {
      [
        msg.DISABLE_PROVIDER,
        msg.ENABLE_PROVIDER,
        msg.EXPIRE_QUERY_CACHE,
        msg.FORCE_WHATSNEW_PANEL,
        msg.IMPRESSION,
        msg.RESET_PROVIDER_PREF,
        msg.SET_PROVIDER_USER_PREF,
        msg.USER_ACTION,
      ].forEach(type => {
        it(`type ${type}`, () => {
          asRouterChild.asRouterMessage({
            type,
            data: {
              something: 1,
            },
          });
          sandbox.assert.calledOnce(asRouterChild.sendAsyncMessage);
          sandbox.assert.calledWith(asRouterChild.sendAsyncMessage, type, {
            something: 1,
          });
        });
      });
    });
  });
});
