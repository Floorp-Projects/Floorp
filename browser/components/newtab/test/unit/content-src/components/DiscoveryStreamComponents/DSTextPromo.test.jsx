import { DSTextPromo } from "content-src/components/DiscoveryStreamComponents/DSTextPromo/DSTextPromo";
import React from "react";
import { shallow } from "enzyme";

describe("<DSTextPromo>", () => {
  let wrapper;
  let sandbox;
  let dispatchStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    wrapper = shallow(
      <DSTextPromo
        data={{
          spocs: [
            {
              shim: { impression: "1234" },
              id: "1234",
            },
          ],
        }}
        type="TEXTPROMO"
        dispatch={dispatchStub}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-text-promo").exists());
  });

  it("should render a header", () => {
    wrapper.setProps({ header: "foo" });
    assert.ok(wrapper.find(".text").exists());
  });

  it("should render a subtitle", () => {
    wrapper.setProps({ subtitle: "foo" });
    assert.ok(wrapper.find(".subtitle").exists());
  });

  it("should dispatch a click event on click", () => {
    wrapper.instance().onLinkClick();

    assert.calledTwice(dispatchStub);
    assert.deepEqual(dispatchStub.firstCall.args[0].data, {
      event: "CLICK",
      source: "TEXTPROMO",
      action_position: 0,
    });
    assert.deepEqual(dispatchStub.secondCall.args[0].data, {
      source: "TEXTPROMO",
      click: 0,
      tiles: [{ id: "1234", pos: 0 }],
    });
  });

  it("should dispath telemety events on dismiss", () => {
    wrapper.instance().onDismissClick();

    const firstCall = dispatchStub.getCall(0);
    const secondCall = dispatchStub.getCall(1);
    const thirdCall = dispatchStub.getCall(2);

    assert.equal(firstCall.args[0].type, "BLOCK_URL");
    assert.deepEqual(firstCall.args[0].data, [
      {
        url: undefined,
        pocket_id: undefined,
        isSponsoredTopSite: undefined,
      },
    ]);

    assert.equal(secondCall.args[0].type, "DISCOVERY_STREAM_USER_EVENT");
    assert.deepEqual(secondCall.args[0].data, {
      event: "BLOCK",
      source: "TEXTPROMO",
      action_position: 0,
    });

    assert.equal(thirdCall.args[0].type, "TELEMETRY_IMPRESSION_STATS");
    assert.deepEqual(thirdCall.args[0].data, {
      source: "TEXTPROMO",
      block: 0,
      tiles: [{ id: "1234", pos: 0 }],
    });
  });
});
