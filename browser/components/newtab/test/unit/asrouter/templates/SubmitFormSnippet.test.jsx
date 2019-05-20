import {mount} from "enzyme";
import React from "react";
import {RichText} from "content-src/asrouter/components/RichText/RichText.jsx";
import schema from "content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";
import {SubmitFormSnippet} from "content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.jsx";

const DEFAULT_CONTENT = {
  scene1_text: "foo",
  scene2_text: "bar",
  scene1_button_label: "Sign Up",
  form_action: "foo.com",
  hidden_inputs: {"foo": "foo"},
  error_text: "error",
  success_text: "success",
};

describe("SubmitFormSnippet", () => {
  let sandbox;
  let onBlockStub;

  /**
   * mountAndCheckProps - Mounts a SubmitFormSnippet with DEFAULT_CONTENT extended with any props
   *                      passed in the content param and validates props against the schema.
   * @param {obj} content Object containing custom message content (e.g. {text, icon, title})
   * @returns enzyme wrapper for SubmitFormSnippet
   */
  function mountAndCheckProps(content = {}) {
    const props = {
      content: Object.assign({}, DEFAULT_CONTENT, content),
      onBlock: onBlockStub,
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
      form_method: "POST",
    };
    assert.jsonSchema(props.content, schema);
    return mount(<SubmitFormSnippet {...props} />);
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    onBlockStub = sandbox.stub();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render .text", () => {
    const wrapper = mountAndCheckProps({scene1_text: "bar"});
    assert.equal(wrapper.find(".body").text(), "bar");
  });
  it("should not render title element if no .title prop is supplied", () => {
    const wrapper = mountAndCheckProps();
    assert.lengthOf(wrapper.find(".title"), 0);
  });
  it("should render .title", () => {
    const wrapper = mountAndCheckProps({scene1_title: "Foo"});
    assert.equal(wrapper.find(".title").text().trim(), "Foo");
  });
  it("should render light-theme .icon", () => {
    const wrapper = mountAndCheckProps({scene1_icon: "data:image/gif;base64,R0lGODl"});
    assert.equal(wrapper.find(".icon-light-theme").prop("src"), "data:image/gif;base64,R0lGODl");
  });
  it("should render dark-theme .icon", () => {
    const wrapper = mountAndCheckProps({scene1_icon_dark_theme: "data:image/gif;base64,R0lGODl"});
    assert.equal(wrapper.find(".icon-dark-theme").prop("src"), "data:image/gif;base64,R0lGODl");
  });
  it("should render .button_label and default className", () => {
    const wrapper = mountAndCheckProps({scene1_button_label: "Click here"});

    const button = wrapper.find("button.ASRouterButton");
    assert.equal(button.text(), "Click here");
    assert.equal(button.prop("className"), "ASRouterButton secondary");
  });

  describe("#SignupView", () => {
    let wrapper;
    const fetchOk = {json: () => Promise.resolve({status: "ok"})};
    const fetchFail = {json: () => Promise.resolve({status: "fail"})};

    beforeEach(() => {
      wrapper = mountAndCheckProps({
        scene1_text: "bar",
        scene2_email_placeholder_text: "Email",
        scene2_text: "signup",
      });
    });

    it("should set the input type if provided through props.inputType", () => {
      wrapper.setProps({inputType: "number"});
      wrapper.setState({expanded: true});
      assert.equal(wrapper.find(".mainInput").instance().type, "number");
    });

    it("should validate via props.validateInput if provided", () => {
      function validateInput(value, content) {
        if (content.country === "CA" && value === "poutine") {
          return "";
        }
        return "Must be poutine";
      }
      const setCustomValidity = sandbox.stub();
      wrapper.setProps({validateInput, content: {...DEFAULT_CONTENT, country: "CA"}});
      wrapper.setState({expanded: true});
      const input = wrapper.find(".mainInput");
      input.instance().value = "poutine";
      input.simulate("change", {target: {value: "poutine", setCustomValidity}});
      assert.calledWith(setCustomValidity, "");

      input.instance().value = "fried chicken";
      input.simulate("change", {target: {value: "fried chicken", setCustomValidity}});
      assert.calledWith(setCustomValidity, "Must be poutine");
    });

    it("should show the signup form if state.expanded is true", () => {
      wrapper.setState({expanded: true});

      assert.isTrue(wrapper.find("form").exists());
    });
    it("should dismiss the snippet", () => {
      wrapper.setState({expanded: true});

      wrapper.find(".ASRouterButton.secondary").simulate("click");

      assert.calledOnce(wrapper.props().onDismiss);
    });
    it("should send a DISMISS event ping", () => {
      wrapper.setState({expanded: true});

      wrapper.find(".ASRouterButton.secondary").simulate("click");

      assert.equal(wrapper.props().sendUserActionTelemetry.firstCall.args[0].event, "DISMISS");
    });
    it("should render hidden inputs + email input", () => {
      wrapper.setState({expanded: true});

      assert.lengthOf(wrapper.find("input[type='hidden']"), 1);
    });
    it("should open the SignupView when the action button is clicked", () => {
      assert.isFalse(wrapper.find("form").exists());

      wrapper.find(".ASRouterButton").simulate("click");

      assert.isTrue(wrapper.state().expanded);
      assert.isTrue(wrapper.find("form").exists());
    });
    it("should submit telemetry when the action button is clicked", () => {
      assert.isFalse(wrapper.find("form").exists());

      wrapper.find(".ASRouterButton").simulate("click");

      assert.equal(wrapper.props().sendUserActionTelemetry.firstCall.args[0].value, "scene1-button-learn-more");
    });
    it("should submit form data when submitted", () => {
      sandbox.stub(window, "fetch").resolves(fetchOk);
      wrapper.setState({expanded: true});

      wrapper.find("form").simulate("submit");
      assert.calledOnce(window.fetch);
    });
    it("should send user telemetry when submitted", () => {
      wrapper.setState({expanded: true});

      wrapper.find("form").simulate("submit");

      assert.equal(wrapper.props().sendUserActionTelemetry.firstCall.args[0].value, "conversion-subscribe-activation");
    });
    it("should set signupSuccess when submission status is ok", async () => {
      sandbox.stub(window, "fetch").resolves(fetchOk);
      wrapper.setState({expanded: true});
      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.equal(wrapper.state().signupSuccess, true);
      assert.equal(wrapper.state().signupSubmitted, true);
      assert.calledOnce(onBlockStub);
      assert.calledWithExactly(onBlockStub, {preventDismiss: true});
    });
    it("should send user telemetry when submission status is ok", async () => {
      sandbox.stub(window, "fetch").resolves(fetchOk);
      wrapper.setState({expanded: true});
      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.equal(wrapper.props().sendUserActionTelemetry.secondCall.args[0].value, "subscribe-success");
    });
    it("should not block the snippet if submission failed", async () => {
      sandbox.stub(window, "fetch").resolves(fetchFail);
      wrapper.setState({expanded: true});
      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.equal(wrapper.state().signupSuccess, false);
      assert.equal(wrapper.state().signupSubmitted, true);
      assert.notCalled(onBlockStub);
    });
    it("should not block if do_not_autoblock is true", async () => {
      sandbox.stub(window, "fetch").resolves(fetchOk);
      wrapper = mountAndCheckProps({
        scene1_text: "bar",
        scene2_email_placeholder_text: "Email",
        scene2_text: "signup",
        do_not_autoblock: true,
      });
      wrapper.setState({expanded: true});
      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.equal(wrapper.state().signupSuccess, true);
      assert.equal(wrapper.state().signupSubmitted, true);
      assert.notCalled(onBlockStub);
    });
    it("should send user telemetry if submission failed", async () => {
      sandbox.stub(window, "fetch").resolves(fetchFail);
      wrapper.setState({expanded: true});
      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.equal(wrapper.props().sendUserActionTelemetry.secondCall.args[0].value, "subscribe-error");
    });
    it("should render the signup success message", () => {
      wrapper.setProps({content: {success_text: "success"}});
      wrapper.setState({signupSuccess: true, signupSubmitted: true});

      assert.isTrue(wrapper.find(".submissionStatus").exists());
      assert.propertyVal(wrapper.find(RichText).props(), "localization_id", "success_text");
      assert.propertyVal(wrapper.find(RichText).props(), "success_text", "success");
      assert.isFalse(wrapper.find(".ASRouterButton").exists());
    });
    it("should render the signup error message", () => {
      wrapper.setProps({content: {error_text: "trouble"}});
      wrapper.setState({signupSuccess: false, signupSubmitted: true});

      assert.isTrue(wrapper.find(".submissionStatus").exists());
      assert.propertyVal(wrapper.find(RichText).props(), "localization_id", "error_text");
      assert.propertyVal(wrapper.find(RichText).props(), "error_text", "trouble");
      assert.isTrue(wrapper.find(".ASRouterButton").exists());
    });
    it("should render the button to return to the signup form if there was an error", () => {
      wrapper.setState({signupSubmitted: true, signupSuccess: false});

      assert.isTrue(wrapper.find(".ASRouterButton").exists());
      wrapper.find(".ASRouterButton").simulate("click");

      assert.equal(wrapper.state().signupSubmitted, false);
    });
    it("should not render the privacy notice checkbox if prop is missing", () => {
      wrapper.setState({expanded: true});

      assert.isFalse(wrapper.find(".privacyNotice").exists());
    });
    it("should render the privacy notice checkbox if prop is provided", () => {
      wrapper.setProps({content: {...DEFAULT_CONTENT, scene2_privacy_html: "privacy notice"}});
      wrapper.setState({expanded: true});

      assert.isTrue(wrapper.find(".privacyNotice").exists());
    });
    it("should not call fetch if form_method is GET", async () => {
      sandbox.stub(window, "fetch").resolves(fetchOk);
      wrapper.setProps({form_method: "GET"});
      wrapper.setState({expanded: true});

      await wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.notCalled(window.fetch);
    });
    it("should block the snippet when form_method is GET", () => {
      wrapper.setProps({form_method: "GET"});
      wrapper.setState({expanded: true});

      wrapper.instance().handleSubmit({preventDefault: sandbox.stub()});

      assert.calledOnce(onBlockStub);
      assert.calledWithExactly(onBlockStub, {preventDismiss: true});
    });
  });
});
