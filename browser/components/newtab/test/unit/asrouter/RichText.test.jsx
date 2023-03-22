import {
  convertLinks,
  RichText,
} from "content-src/asrouter/components/RichText/RichText";
import { FluentBundle, FluentResource } from "@fluent/bundle";
import {
  Localized,
  LocalizationProvider,
  ReactLocalization,
} from "@fluent/react";
import { mount } from "enzyme";
import React from "react";

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

describe("convertLinks", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should return an object with anchor elements", () => {
    const cta = {
      url: "https://foo.com",
      metric: "foo",
    };
    const stub = sandbox.stub();
    const result = convertLinks({ cta }, stub);

    assert.property(result, "cta");
    assert.propertyVal(result.cta, "type", "a");
    assert.propertyVal(result.cta.props, "href", cta.url);
    assert.propertyVal(result.cta.props, "data-metric", cta.metric);
    assert.propertyVal(result.cta.props, "onClick", stub);
  });
  it("should return an anchor element without href", () => {
    const cta = {
      url: "https://foo.com",
      metric: "foo",
      action: "OPEN_MENU",
      args: "appMenu",
      entrypoint_name: "entrypoint_name",
      entrypoint_value: "entrypoint_value",
    };
    const stub = sandbox.stub();
    const result = convertLinks({ cta }, stub);

    assert.property(result, "cta");
    assert.propertyVal(result.cta, "type", "a");
    assert.propertyVal(result.cta.props, "href", false);
    assert.propertyVal(result.cta.props, "data-metric", cta.metric);
    assert.propertyVal(result.cta.props, "data-action", cta.action);
    assert.propertyVal(result.cta.props, "data-args", cta.args);
    assert.propertyVal(
      result.cta.props,
      "data-entrypoint_name",
      cta.entrypoint_name
    );
    assert.propertyVal(
      result.cta.props,
      "data-entrypoint_value",
      cta.entrypoint_value
    );
    assert.propertyVal(result.cta.props, "onClick", stub);
  });
  it("should follow openNewWindow prop", () => {
    const cta = { url: "https://foo.com" };
    const newWindow = convertLinks({ cta }, sandbox.stub(), false, true);
    const sameWindow = convertLinks({ cta }, sandbox.stub(), false);

    assert.propertyVal(newWindow.cta.props, "target", "_blank");
    assert.propertyVal(sameWindow.cta.props, "target", "");
  });
  it("should allow for custom elements & styles", () => {
    const wrapper = mount(
      <RichText
        customElements={{ em: <em style={{ color: "#f05" }} /> }}
        text="<em>foo</em>"
        localization_id="text"
      />,
      mockL10nWrapper({ text: "<em>foo</em>" })
    );

    const localized = wrapper.find(Localized);
    assert.propertyVal(localized.props().elems.em.props.style, "color", "#f05");
  });
});
