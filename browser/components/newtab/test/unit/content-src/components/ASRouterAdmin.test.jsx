import {ASRouterAdminInner} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import {GlobalOverrider} from "test/unit/utils";
import React from "react";
import {shallow} from "enzyme";

describe("ASRouterAdmin", () => {
  let globals;
  let sandbox;
  let sendMessageStub;
  let addListenerStub;
  let removeListenerStub;
  let wrapper;
  let FAKE_PROVIDER_PREF = [{
    enabled: true,
    id: "snippets_local_testing",
    localProvider: "SnippetsProvider",
    type: "local",
  }];
  let FAKE_PROVIDER = [{
    enabled: true,
    id: "snippets_local_testing",
    localProvider: "SnippetsProvider",
    messages: [],
    type: "local",
  }];
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.sandbox.create();
    sendMessageStub = sandbox.stub();
    addListenerStub = sandbox.stub();
    removeListenerStub = sandbox.stub();

    globals.set("RPMSendAsyncMessage", sendMessageStub);
    globals.set("RPMAddMessageListener", addListenerStub);
    globals.set("RPMRemoveMessageListener", removeListenerStub);

    wrapper = shallow(<ASRouterAdminInner location={{routes: [""]}} />);
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  it("should render ASRouterAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should send ADMIN_CONNECT_STATE on mount", () => {
    assert.calledOnce(sendMessageStub);
    assert.propertyVal(sendMessageStub.firstCall.args[1], "type", "ADMIN_CONNECT_STATE");
  });
  it("should set a listener on mount", () => {
    assert.calledOnce(addListenerStub);
    assert.calledWithExactly(addListenerStub, sinon.match.string, wrapper.instance().onMessage);
  });
  it("should remove listener on unmount", () => {
    wrapper.unmount();
    assert.calledOnce(removeListenerStub);
  });
  describe("#getSection", () => {
    it("should render a message provider section by default", () => {
      assert.equal(wrapper.find("h2").at(2).text(), "Messages");
    });
    it("should render a targeting section for targeting route", () => {
      wrapper = shallow(<ASRouterAdminInner location={{routes: ["targeting"]}} />);
      assert.equal(wrapper.find("h2").at(0).text(), "Targeting Utilities");
    });
    it("should render a pocket section for pocket route", () => {
      wrapper = shallow(<ASRouterAdminInner location={{routes: ["pocket"]}} Sections={[]} />);
      assert.equal(wrapper.find("h2").at(0).text(), "Pocket");
    });
  });
  describe("#render", () => {
    beforeEach(() => {
      wrapper.setState({
        providerPrefs: [],
        providers: [],
        userPrefs: {},
      });
    });
    describe("#renderProviders", () => {
      it("should render the provider", () => {
        wrapper.setState({
          providerPrefs: FAKE_PROVIDER_PREF,
          providers: FAKE_PROVIDER,
        });

        // Header + 1 item
        assert.lengthOf(wrapper.find(".message-item"), 2);
      });
    });
    describe("#renderMessages", () => {
      beforeEach(() => {
        wrapper.setState({
          messageFilter: "all",
          messageBlockList: [],
          messageImpressions: {foo: 2},
        });
      });
      it("should render a message when no filtering is applied", () => {
        wrapper.setState({
          messages: [{id: "foo"}],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.primary").simulate("click");
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(sendMessageStub.secondCall.args[1], "type", "BLOCK_MESSAGE_BY_ID");
        assert.propertyVal(sendMessageStub.secondCall.args[1].data, "id", "foo");
      });
      it("should render a blocked message", () => {
        wrapper.setState({
          messages: [{id: "foo"}],
          messageBlockList: ["foo"],
        });
        assert.lengthOf(wrapper.find(".message-item.blocked"), 1);
        wrapper.find(".message-item.blocked button").simulate("click");
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(sendMessageStub.secondCall.args[1], "type", "UNBLOCK_MESSAGE_BY_ID");
        assert.propertyVal(sendMessageStub.secondCall.args[1].data, "id", "foo");
      });
      it("should render a message if provider matches filter", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [{id: "foo", provider: "messageProvider"}],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
      });
      it("should override with the selected message", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [{id: "foo", provider: "messageProvider"}],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button:not(.primary)").simulate("click");
        // first call is ADMIN_CONNECT_STATE
        assert.propertyVal(sendMessageStub.secondCall.args[1], "type", "OVERRIDE_MESSAGE");
        assert.propertyVal(sendMessageStub.secondCall.args[1].data, "id", "foo");
      });
      it("should hide message if provider filter changes", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [{id: "foo", provider: "messageProvider"}],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);

        wrapper.find("select").simulate("change", {target: {value: "bar"}});

        assert.lengthOf(wrapper.find(".message-id"), 0);
      });
    });
  });
});
