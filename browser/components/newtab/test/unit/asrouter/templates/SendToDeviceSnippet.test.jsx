import { mount } from "enzyme";
import React from "react";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { LocalizationProvider, ReactLocalization } from "@fluent/react";
import schema from "content-src/asrouter/templates/SendToDeviceSnippet/SendToDeviceSnippet.schema.json";
import {
  SendToDeviceSnippet,
  SendToDeviceScene2Snippet,
} from "content-src/asrouter/templates/SendToDeviceSnippet/SendToDeviceSnippet";
import { SnippetsTestMessageProvider } from "lib/SnippetsTestMessageProvider.sys.mjs";

async function testBodyContains(body, key, value) {
  const regex = new RegExp(
    `Content-Disposition: form-data; name="${key}"${value}`
  );
  const match = regex.exec(body);
  return match;
}

/**
 * Simulates opening the second panel (form view), filling in the input, and submitting
 * @param {EnzymeWrapper} wrapper A SendToDevice wrapper
 * @param {string} value Email or phone number
 * @param {function?} setCustomValidity setCustomValidity stub
 */
function openFormAndSetValue(wrapper, value, setCustomValidity = () => {}) {
  // expand
  wrapper.find(".ASRouterButton").simulate("click");
  // Fill in email
  const input = wrapper.find(".mainInput");
  input.instance().value = value;
  input.simulate("change", { target: { value, setCustomValidity } });
  wrapper.find("form").simulate("submit");
}

describe("SendToDeviceSnippet", () => {
  let sandbox;
  let fetchStub;
  let jsonResponse;
  let DEFAULT_CONTENT;
  let DEFAULT_SCENE2_CONTENT;

  function mockL10nWrapper(content) {
    const bundle = new FluentBundle("en-US");
    for (const [id, value] of Object.entries(content)) {
      if (typeof value === "string") {
        bundle.addResource(new FluentResource(`${id} = ${value}`));
      }
    }
    const l10n = new ReactLocalization([bundle]);
    return {
      wrappingComponent: LocalizationProvider,
      wrappingComponentProps: { l10n },
    };
  }

  function mountAndCheckProps(content = {}) {
    const props = {
      id: "foo123",
      content: Object.assign({}, DEFAULT_CONTENT, content),
      onBlock() {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
    };
    const comp = mount(
      <SendToDeviceSnippet {...props} />,
      mockL10nWrapper(props.content)
    );
    // Check schema with the final props the component receives (including defaults)
    assert.jsonSchema(comp.children().get(0).props.content, schema);
    return comp;
  }

  beforeEach(async () => {
    DEFAULT_CONTENT = (await SnippetsTestMessageProvider.getMessages()).find(
      msg => msg.template === "send_to_device_snippet"
    ).content;
    DEFAULT_SCENE2_CONTENT = (
      await SnippetsTestMessageProvider.getMessages()
    ).find(msg => msg.template === "send_to_device_scene2_snippet").content;
    sandbox = sinon.createSandbox();
    jsonResponse = { status: "ok" };
    fetchStub = sandbox
      .stub(global, "fetch")
      .returns(Promise.resolve({ json: () => Promise.resolve(jsonResponse) }));
  });
  afterEach(() => {
    sandbox.restore();
  });

  it("should have the correct defaults", () => {
    const defaults = {
      id: "foo123",
      onBlock() {},
      content: {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
      form_method: "POST",
    };
    const wrapper = mount(
      <SendToDeviceSnippet {...defaults} />,
      mockL10nWrapper(DEFAULT_CONTENT)
    );
    // SendToDeviceSnippet is a wrapper around SubmitFormSnippet
    const { props } = wrapper.children().get(0);

    const defaultProperties = Object.keys(schema.properties).filter(
      prop => schema.properties[prop].default
    );
    assert.lengthOf(defaultProperties, 7);
    defaultProperties.forEach(prop =>
      assert.propertyVal(props.content, prop, schema.properties[prop].default)
    );

    const defaultHiddenProperties = Object.keys(
      schema.properties.hidden_inputs.properties
    ).filter(prop => schema.properties.hidden_inputs.properties[prop].default);
    assert.lengthOf(defaultHiddenProperties, 0);
  });

  describe("form input", () => {
    it("should set the input type to text if content.include_sms is true", () => {
      const wrapper = mountAndCheckProps({ include_sms: true });
      wrapper.find(".ASRouterButton").simulate("click");
      assert.equal(wrapper.find(".mainInput").instance().type, "text");
    });
    it("should set the input type to email if content.include_sms is false", () => {
      const wrapper = mountAndCheckProps({ include_sms: false });
      wrapper.find(".ASRouterButton").simulate("click");
      assert.equal(wrapper.find(".mainInput").instance().type, "email");
    });
    it("should validate the input with isEmailOrPhoneNumber if include_sms is true", () => {
      const wrapper = mountAndCheckProps({ include_sms: true });
      const setCustomValidity = sandbox.stub();
      openFormAndSetValue(wrapper, "foo", setCustomValidity);
      assert.calledWith(
        setCustomValidity,
        "Must be an email or a phone number."
      );
    });
    it("should not custom validate the input if include_sms is false", () => {
      const wrapper = mountAndCheckProps({ include_sms: false });
      const setCustomValidity = sandbox.stub();
      openFormAndSetValue(wrapper, "foo", setCustomValidity);
      assert.notCalled(setCustomValidity);
    });
  });

  describe("submitting", () => {
    it("should send the right information to basket.mozilla.org/news/subscribe for an email", async () => {
      const wrapper = mountAndCheckProps({
        locale: "fr-CA",
        include_sms: true,
        message_id_email: "foo",
      });

      openFormAndSetValue(wrapper, "foo@bar.com");
      wrapper.find("form").simulate("submit");

      assert.calledOnce(fetchStub);
      const [request] = fetchStub.firstCall.args;

      assert.equal(request.url, "https://basket.mozilla.org/news/subscribe/");
      const body = await request.text();
      assert.ok(testBodyContains(body, "email", "foo@bar.com"), "has email");
      assert.ok(testBodyContains(body, "lang", "fr-CA"), "has lang");
      assert.ok(
        testBodyContains(body, "newsletters", "foo"),
        "has newsletters"
      );
      assert.ok(
        testBodyContains(body, "source_url", "foo"),
        "https%3A%2F%2Fsnippets.mozilla.com%2Fshow%2Ffoo123"
      );
    });
    it("should send the right information for an sms", async () => {
      const wrapper = mountAndCheckProps({
        locale: "fr-CA",
        include_sms: true,
        message_id_sms: "foo",
        country: "CA",
      });

      openFormAndSetValue(wrapper, "5371283767");
      wrapper.find("form").simulate("submit");

      assert.calledOnce(fetchStub);
      const [request] = fetchStub.firstCall.args;

      assert.equal(
        request.url,
        "https://basket.mozilla.org/news/subscribe_sms/"
      );
      const body = await request.text();
      assert.ok(
        testBodyContains(body, "mobile_number", "5371283767"),
        "has number"
      );
      assert.ok(testBodyContains(body, "lang", "fr-CA"), "has lang");
      assert.ok(testBodyContains(body, "country", "CA"), "CA");
      assert.ok(testBodyContains(body, "msg_name", "foo"), "has msg_name");
    });
  });

  describe("SendToDeviceScene2Snippet", () => {
    function mountWithProps(content = {}) {
      const props = {
        id: "foo123",
        content: Object.assign({}, DEFAULT_SCENE2_CONTENT, content),
        onBlock() {},
        onDismiss: sandbox.stub(),
        sendUserActionTelemetry: sandbox.stub(),
        onAction: sandbox.stub(),
      };
      return mount(
        <SendToDeviceScene2Snippet {...props} />,
        mockL10nWrapper(props.content)
      );
    }

    it("should render scene 2", () => {
      const wrapper = mountWithProps();

      assert.lengthOf(wrapper.find(".scene2Icon"), 1, "Found scene 2 icon");
      assert.lengthOf(
        wrapper.find(".scene2Title"),
        0,
        "Should not have a large header"
      );
    });
    it("should have block button", () => {
      const wrapper = mountWithProps();

      assert.lengthOf(
        wrapper.find(".blockButton"),
        1,
        "Found the block button"
      );
    });
    it("should render title text", () => {
      const wrapper = mountWithProps();

      assert.lengthOf(
        wrapper.find(".section-title-text"),
        1,
        "Found the section title"
      );
      assert.lengthOf(
        wrapper.find(".section-title .icon"),
        2, // light and dark theme
        "Found scene 2 title"
      );
    });
    it("should wrap the header in an anchor tag if condition is defined", () => {
      const sectionTitleProp = {
        section_title_url: "https://support.mozilla.org",
      };
      let wrapper = mountWithProps(sectionTitleProp);

      const element = wrapper.find(".section-title a");
      assert.lengthOf(element, 1);
    });
    it("should render a header without an anchor", () => {
      const sectionTitleProp = {
        section_title_url: undefined,
      };
      let wrapper = mountWithProps(sectionTitleProp);
      assert.lengthOf(wrapper.find(".section-title a"), 0);
      assert.equal(
        wrapper.find(".section-title").instance().innerText,
        DEFAULT_SCENE2_CONTENT.section_title_text
      );
    });
  });
});
