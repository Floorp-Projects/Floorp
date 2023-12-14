import { ASRouterAdminInner } from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import { ASRouterUtils } from "content-src/asrouter/asrouter-utils";
import { GlobalOverrider } from "test/unit/utils";
import React from "react";
import { shallow } from "enzyme";

describe("ASRouterAdmin", () => {
  let globalOverrider;
  let sandbox;
  let wrapper;
  let globals;
  let FAKE_PROVIDER_PREF = [
    {
      enabled: true,
      id: "local_testing",
      localProvider: "TestProvider",
      type: "local",
    },
  ];
  let FAKE_PROVIDER = [
    {
      enabled: true,
      id: "local_testing",
      localProvider: "TestProvider",
      messages: [],
      type: "local",
    },
  ];
  beforeEach(() => {
    globalOverrider = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    sandbox.stub(ASRouterUtils, "getPreviewEndpoint").returns("foo");
    globals = {
      ASRouterMessage: sandbox.stub().resolves(),
      ASRouterAddParentListener: sandbox.stub(),
      ASRouterRemoveParentListener: sandbox.stub(),
    };
    globalOverrider.set(globals);
    wrapper = shallow(<ASRouterAdminInner location={{ routes: [""] }} />);
    wrapper.setState({ devtoolsEnabled: true });
  });
  afterEach(() => {
    sandbox.restore();
    globalOverrider.restore();
  });
  it("should render ASRouterAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should send ADMIN_CONNECT_STATE on mount", () => {
    assert.calledOnce(globals.ASRouterMessage);
    assert.calledWith(globals.ASRouterMessage, {
      type: "ADMIN_CONNECT_STATE",
      data: { endpoint: "foo" },
    });
  });
  describe("#getSection", () => {
    it("should render a message provider section by default", () => {
      assert.equal(wrapper.find("h2").at(1).text(), "Messages");
    });
    it("should render a targeting section for targeting route", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["targeting"] }} />
      );
      wrapper.setState({ devtoolsEnabled: true });
      assert.equal(wrapper.find("h2").at(0).text(), "Targeting Utilities");
    });
    it("should render two error messages", () => {
      wrapper = shallow(
        <ASRouterAdminInner location={{ routes: ["errors"] }} Sections={[]} />
      );
      wrapper.setState({ devtoolsEnabled: true });
      const firstError = {
        timestamp: Date.now() + 100,
        error: { message: "first" },
      };
      const secondError = {
        timestamp: Date.now(),
        error: { message: "second" },
      };
      wrapper.setState({
        providers: [{ id: "foo", errors: [firstError, secondError] }],
      });

      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(0).text(),
        "foo"
      );
      assert.lengthOf(wrapper.find("tbody tr"), 2);
      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(1).text(),
        secondError.error.message
      );
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
        sandbox.stub(ASRouterUtils, "blockById").resolves();
        sandbox.stub(ASRouterUtils, "unblockById").resolves();
        sandbox.stub(ASRouterUtils, "overrideMessage").resolves({ foo: "bar" });
        sandbox.stub(ASRouterUtils, "sendMessage").resolves();
        wrapper.setState({
          messageFilter: "all",
          messageBlockList: [],
          messageImpressions: { foo: 2 },
          groups: [{ id: "messageProvider", enabled: true }],
          providers: [{ id: "messageProvider", enabled: true }],
        });
      });
      it("should render a message when no filtering is applied", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.primary").simulate("click");
        assert.calledOnce(ASRouterUtils.blockById);
        assert.calledWith(ASRouterUtils.blockById, "foo");
      });
      it("should render a blocked message", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              groups: ["messageProvider"],
              provider: "messageProvider",
            },
          ],
          messageBlockList: ["foo"],
        });
        assert.lengthOf(wrapper.find(".message-item.blocked"), 1);
        wrapper.find(".message-item.blocked button").simulate("click");
        assert.calledOnce(ASRouterUtils.unblockById);
        assert.calledWith(ASRouterUtils.unblockById, "foo");
      });
      it("should render a message if provider matches filter", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
      });
      it("should override with the selected message", async () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.show").simulate("click");
        assert.calledOnce(ASRouterUtils.overrideMessage);
        assert.calledWith(ASRouterUtils.overrideMessage, "foo");
        await ASRouterUtils.overrideMessage();
        assert.equal(wrapper.state().foo, "bar");
      });
      it("should hide message if provider filter changes", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);

        wrapper.find("select").simulate("change", { target: { value: "bar" } });

        assert.lengthOf(wrapper.find(".message-id"), 0);
      });
      it("should not display Reset All button if provider filter value is set to all or test providers", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".messages-reset"), 1);
        wrapper.find("select").simulate("change", { target: { value: "all" } });

        assert.lengthOf(wrapper.find(".messages-reset"), 0);

        wrapper
          .find("select")
          .simulate("change", { target: { value: "test_local_testing" } });
        assert.lengthOf(wrapper.find(".messages-reset"), 0);
      });
      it("should trigger disable and enable provider on Reset All button click", () => {
        wrapper.setState({
          messageFilter: "messageProvider",
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
          providerPrefs: [
            {
              id: "messageProvider",
            },
          ],
        });
        wrapper.find(".messages-reset").simulate("click");
        assert.calledTwice(ASRouterUtils.sendMessage);
        assert.calledWith(ASRouterUtils.sendMessage, {
          type: "DISABLE_PROVIDER",
          data: "messageProvider",
        });
        assert.calledWith(ASRouterUtils.sendMessage, {
          type: "ENABLE_PROVIDER",
          data: "messageProvider",
        });
      });
    });
  });
});
