import { EOYSnippet } from "content-src/asrouter/templates/EOYSnippet/EOYSnippet";
import { GlobalOverrider } from "test/unit/utils";
import { mount } from "enzyme";
import React from "react";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import { LocalizationProvider, ReactLocalization } from "@fluent/react";
import schema from "content-src/asrouter/templates/EOYSnippet/EOYSnippet.schema.json";

const DEFAULT_CONTENT = {
  text: "foo",
  donation_amount_first: 50,
  donation_amount_second: 25,
  donation_amount_third: 10,
  donation_amount_fourth: 5,
  donation_form_url: "https://submit.form",
  button_label: "Donate",
};

describe("EOYSnippet", () => {
  let sandbox;
  let wrapper;

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

  /**
   * mountAndCheckProps - Mounts a EOYSnippet with DEFAULT_CONTENT extended with any props
   *                      passed in the content param and validates props against the schema.
   * @param {obj} content Object containing custom message content (e.g. {text, icon, title})
   * @returns enzyme wrapper for EOYSnippet
   */
  function mountAndCheckProps(content = {}, provider = "test-provider") {
    const props = {
      content: Object.assign({}, DEFAULT_CONTENT, content),
      provider,
      onAction: sandbox.stub(),
      onBlock: sandbox.stub(),
      sendClick: sandbox.stub(),
    };
    const comp = mount(
      <EOYSnippet {...props} />,
      mockL10nWrapper(props.content)
    );
    // Check schema with the final props the component receives (including defaults)
    assert.jsonSchema(comp.children().get(0).props.content, schema);
    return comp;
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    wrapper = mountAndCheckProps();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should have the correct defaults", () => {
    wrapper = mountAndCheckProps();
    // SendToDeviceSnippet is a wrapper around SubmitFormSnippet
    const { props } = wrapper.children().get(0);

    const defaultProperties = Object.keys(schema.properties).filter(
      prop => schema.properties[prop].default
    );
    assert.lengthOf(defaultProperties, 4);
    defaultProperties.forEach(prop =>
      assert.propertyVal(props.content, prop, schema.properties[prop].default)
    );
  });

  it("should render 4 donation options", () => {
    assert.lengthOf(wrapper.find("input[type='radio']"), 4);
  });

  it("should have a data-metric field", () => {
    assert.ok(wrapper.find("form[data-metric='EOYSnippetForm']").exists());
  });

  it("should select the second donation option", () => {
    wrapper = mountAndCheckProps({ selected_button: "donation_amount_second" });

    assert.propertyVal(
      wrapper.find("input[type='radio']").get(1).props,
      "defaultChecked",
      true
    );
  });

  it("should set frequency value to monthly", () => {
    const form = wrapper.find("form").instance();
    assert.equal(form.querySelector("[name='frequency']").value, "single");

    form.querySelector("#monthly-checkbox").checked = true;
    wrapper.find("form").simulate("submit");

    assert.equal(form.querySelector("[name='frequency']").value, "monthly");
  });

  it("should block after submitting the form", () => {
    const onBlockStub = sandbox.stub();
    wrapper.setProps({ onBlock: onBlockStub });

    wrapper.find("form").simulate("submit");

    assert.calledOnce(onBlockStub);
  });

  it("should not block if do_not_autoblock is true", () => {
    const onBlockStub = sandbox.stub();
    wrapper = mountAndCheckProps({ do_not_autoblock: true });
    wrapper.setProps({ onBlock: onBlockStub });

    wrapper.find("form").simulate("submit");

    assert.notCalled(onBlockStub);
  });

  it("should report form submissions", () => {
    wrapper = mountAndCheckProps();
    const { sendClick } = wrapper.props();

    wrapper.find("form").simulate("submit");

    assert.calledOnce(sendClick);
    assert.equal(
      sendClick.firstCall.args[0].target.dataset.metric,
      "EOYSnippetForm"
    );
  });

  it("it should preserve URL GET params as hidden inputs", () => {
    wrapper = mountAndCheckProps({
      donation_form_url:
        "https://donate.mozilla.org/pl/?utm_source=desktop-snippet&amp;utm_medium=snippet&amp;utm_campaign=donate&amp;utm_term=7556",
    });

    const hiddenInputs = wrapper.find("input[type='hidden']");

    assert.propertyVal(
      hiddenInputs.find("[name='utm_source']").props(),
      "value",
      "desktop-snippet"
    );
    assert.propertyVal(
      hiddenInputs.find("[name='amp;utm_medium']").props(),
      "value",
      "snippet"
    );
    assert.propertyVal(
      hiddenInputs.find("[name='amp;utm_campaign']").props(),
      "value",
      "donate"
    );
    assert.propertyVal(
      hiddenInputs.find("[name='amp;utm_term']").props(),
      "value",
      "7556"
    );
  });

  describe("locale", () => {
    let stub;
    let globals;
    beforeEach(() => {
      globals = new GlobalOverrider();
      stub = sandbox.stub().returns({ format: () => {} });

      globals = new GlobalOverrider();
      globals.set({ Intl: { NumberFormat: stub } });
    });
    afterEach(() => {
      globals.restore();
    });

    it("should use content.locale for Intl", () => {
      // triggers component rendering and calls the function we're testing
      wrapper.setProps({
        content: {
          locale: "locale-foo",
          donation_form_url: DEFAULT_CONTENT.donation_form_url,
        },
      });

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, "locale-foo", sinon.match.object);
    });

    it("should use navigator.language as locale fallback", () => {
      // triggers component rendering and calls the function we're testing
      wrapper.setProps({
        content: {
          locale: null,
          donation_form_url: DEFAULT_CONTENT.donation_form_url,
        },
      });

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, navigator.language, sinon.match.object);
    });
  });
});
