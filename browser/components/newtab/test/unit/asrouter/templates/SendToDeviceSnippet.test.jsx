import {mount} from "enzyme";
import React from "react";
import schema from "content-src/asrouter/templates/SubmitFormSnippet/SubmitFormSnippet.schema.json";
import {SendToDeviceSnippet} from "content-src/asrouter/templates/SendToDeviceSnippet/SendToDeviceSnippet";
import {SnippetsTestMessageProvider} from "lib/SnippetsTestMessageProvider.jsm";

const DEFAULT_CONTENT = SnippetsTestMessageProvider.getMessages().find(msg => msg.template === "send_to_device_snippet").content;

async function testBodyContains(body, key, value) {
  const regex = new RegExp(`Content-Disposition: form-data; name="${key}"${value}`);
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
  const input =  wrapper.find(".mainInput");
  input.instance().value = value;
  input.simulate("change", {target: {value, setCustomValidity}});
  wrapper.find("form").simulate("submit");
}

describe("SendToDeviceSnippet", () => {
  let sandbox;
  let fetchStub;
  let jsonResponse;

  function mountAndCheckProps(content = {}) {
    const props = {
      id: "foo123",
      content: Object.assign({}, DEFAULT_CONTENT, content),
      onBlock() {},
      onDismiss: sandbox.stub(),
      sendUserActionTelemetry: sandbox.stub(),
      onAction: sandbox.stub(),
      form_method: "POST",
    };
    assert.jsonSchema(props.content, schema);
    return mount(<SendToDeviceSnippet {...props} />);
  }

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    jsonResponse = {status: "ok"};
    fetchStub = sandbox.stub(global, "fetch")
      .returns(Promise.resolve({json: () => Promise.resolve(jsonResponse)}));
  });
  afterEach(() => {
    sandbox.restore();
  });

  describe("form input", () => {
    it("should set the input type to text if content.include_sms is true", () => {
      const wrapper = mountAndCheckProps({include_sms: true});
      wrapper.find(".ASRouterButton").simulate("click");
      assert.equal(wrapper.find(".mainInput").instance().type, "text");
    });
    it("should set the input type to email if content.include_sms is false", () => {
      const wrapper = mountAndCheckProps({include_sms: false});
      wrapper.find(".ASRouterButton").simulate("click");
      assert.equal(wrapper.find(".mainInput").instance().type, "email");
    });
    it("should validate the input with isEmailOrPhoneNumber if include_sms is true", () => {
      const wrapper = mountAndCheckProps({include_sms: true});
      const setCustomValidity = sandbox.stub();
      openFormAndSetValue(wrapper, "foo", setCustomValidity);
      assert.calledWith(setCustomValidity, "Must be an email or a phone number.");
    });
    it("should not custom validate the input if include_sms is false", () => {
      const wrapper = mountAndCheckProps({include_sms: false});
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
      assert.ok(testBodyContains(body, "newsletters", "foo"), "has newsletters");
      assert.ok(testBodyContains(body, "source_url", "foo"), "https%3A%2F%2Fsnippets.mozilla.com%2Fshow%2Ffoo123");
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

      assert.equal(request.url, "https://basket.mozilla.org/news/subscribe_sms/");
      const body = await request.text();
      assert.ok(testBodyContains(body, "mobile_number", "5371283767"), "has number");
      assert.ok(testBodyContains(body, "lang", "fr-CA"), "has lang");
      assert.ok(testBodyContains(body, "country", "CA"), "CA");
      assert.ok(testBodyContains(body, "msg_name", "foo"), "has msg_name");
    });
  });
});
