import {
  _DiscoveryStreamBase as DiscoveryStreamBase,
  isAllowedCSS,
} from "content-src/components/DiscoveryStreamBase/DiscoveryStreamBase";
import { GlobalOverrider } from "test/unit/utils";
import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { CollapsibleSection } from "content-src/components/CollapsibleSection/CollapsibleSection";
import { DSMessage } from "content-src/components/DiscoveryStreamComponents/DSMessage/DSMessage";
import { Hero } from "content-src/components/DiscoveryStreamComponents/Hero/Hero";
import { HorizontalRule } from "content-src/components/DiscoveryStreamComponents/HorizontalRule/HorizontalRule";
import { List } from "content-src/components/DiscoveryStreamComponents/List/List";
import { Navigation } from "content-src/components/DiscoveryStreamComponents/Navigation/Navigation";
import React from "react";
import { shallow } from "enzyme";
import { SectionTitle } from "content-src/components/DiscoveryStreamComponents/SectionTitle/SectionTitle";
import { TopSites } from "content-src/components/DiscoveryStreamComponents/TopSites/TopSites";

describe("<isAllowedCSS>", () => {
  it("should allow colors", () => {
    assert.isTrue(isAllowedCSS("color", "red"));
  });

  it("should allow chrome urls", () => {
    assert.isTrue(
      isAllowedCSS(
        "background-image",
        `url("chrome://activity-stream/content/data/content/assets/glyph-info-16.svg")`
      )
    );
  });

  it("should allow chrome urls", () => {
    assert.isTrue(
      isAllowedCSS(
        "background-image",
        `url("chrome://browser/skin/history.svg")`
      )
    );
  });

  it("should allow allowed https urls", () => {
    assert.isTrue(
      isAllowedCSS(
        "background-image",
        `url("https://img-getpocket.cdn.mozilla.net/media/image.png")`
      )
    );
  });

  it("should disallow other https urls", () => {
    assert.isFalse(
      isAllowedCSS(
        "background-image",
        `url("https://mozilla.org/media/image.png")`
      )
    );
  });

  it("should disallow other protocols", () => {
    assert.isFalse(
      isAllowedCSS(
        "background-image",
        `url("ftp://mozilla.org/media/image.png")`
      )
    );
  });

  it("should allow allowed multiple valid urls", () => {
    assert.isTrue(
      isAllowedCSS(
        "background-image",
        `url("https://img-getpocket.cdn.mozilla.net/media/image.png"), url("chrome://browser/skin/history.svg")`
      )
    );
  });

  it("should disallow if any invaild", () => {
    assert.isFalse(
      isAllowedCSS(
        "background-image",
        `url("chrome://browser/skin/history.svg"), url("ftp://mozilla.org/media/image.png")`
      )
    );
  });
});

describe("<DiscoveryStreamBase>", () => {
  let wrapper;
  let globals;
  let sandbox;

  function mountComponent(props = {}) {
    const defaultProps = {
      config: { collapsible: true },
      layout: [],
      feeds: { loaded: true },
      spocs: {
        loaded: true,
        data: { spocs: null },
      },
      ...props,
    };
    return shallow(
      <DiscoveryStreamBase
        locale="en-US"
        DiscoveryStream={defaultProps}
        Prefs={{
          values: {
            "feeds.section.topstories": true,
            "feeds.system.topstories": true,
            "feeds.topsites": true,
          },
        }}
        App={{
          locale: "en-US",
        }}
        document={{
          documentElement: { lang: "en-US" },
        }}
        Sections={[
          {
            id: "topstories",
            learnMore: { link: {} },
            pref: {},
          },
        ]}
      />
    );
  }

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    wrapper = mountComponent();
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  it("should render something if spocs are not loaded", () => {
    wrapper = mountComponent({
      spocs: { loaded: false, data: { spocs: null } },
    });

    assert.notEqual(wrapper.type(), null);
  });

  it("should render something if feeds are not loaded", () => {
    wrapper = mountComponent({ feeds: { loaded: false } });

    assert.notEqual(wrapper.type(), null);
  });

  it("should render nothing with no layout", () => {
    assert.ok(wrapper.exists());
    assert.isEmpty(wrapper.children());
  });

  it("should render a HorizontalRule component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ type: "HorizontalRule" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      HorizontalRule
    );
  });

  it("should render a List component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "List" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      List
    );
  });

  it("should render a Hero component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "Hero" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      Hero
    );
  });

  it("should render a CardGrid component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "CardGrid" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      CardGrid
    );
  });

  it("should render a Navigation component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "Navigation" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      Navigation
    );
  });

  it("should render nothing if there was only a Message", () => {
    wrapper = mountComponent({
      layout: [
        { components: [{ header: {}, properties: {}, type: "Message" }] },
      ],
    });

    assert.isEmpty(wrapper.children());
  });

  it("should render a regular Message when not collapsible", () => {
    wrapper = mountComponent({
      config: { collapsible: false },
      layout: [
        { components: [{ header: {}, properties: {}, type: "Message" }] },
      ],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      DSMessage
    );
  });

  it("should convert first Message component to CollapsibleSection", () => {
    wrapper = mountComponent({
      layout: [
        {
          components: [
            { header: {}, properties: {}, type: "Message" },
            { type: "HorizontalRule" },
          ],
        },
      ],
    });

    assert.equal(
      wrapper
        .children()
        .at(0)
        .type(),
      CollapsibleSection
    );
    assert.equal(
      wrapper
        .children()
        .at(0)
        .props().eventSource,
      "CARDGRID"
    );
  });

  it("should render a Message component", () => {
    wrapper = mountComponent({
      layout: [
        {
          components: [
            { header: {}, type: "Message" },
            { properties: {}, type: "Message" },
          ],
        },
      ],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      DSMessage
    );
  });

  it("should render a SectionTitle component", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "SectionTitle" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      SectionTitle
    );
  });

  it("should render TopSites", () => {
    wrapper = mountComponent({
      layout: [{ components: [{ properties: {}, type: "TopSites" }] }],
    });

    assert.equal(
      wrapper
        .find(".ds-column-grid div")
        .children()
        .at(0)
        .type(),
      TopSites
    );
  });

  describe("#onStyleMount", () => {
    let parseStub;

    beforeEach(() => {
      parseStub = sandbox.stub();
      globals.set("JSON", { parse: parseStub });
    });

    afterEach(() => {
      sandbox.restore();
      globals.restore();
    });

    it("should return if no style", () => {
      assert.isUndefined(wrapper.instance().onStyleMount());
      assert.notCalled(parseStub);
    });

    it("should insert rules", () => {
      const sheetStub = { insertRule: sandbox.stub(), cssRules: [{}] };
      parseStub.returns([
        [
          null,
          {
            ".ds-message": "margin-bottom: -20px",
          },
          null,
          null,
        ],
      ]);
      wrapper.instance().onStyleMount({ sheet: sheetStub, dataset: {} });

      assert.calledOnce(sheetStub.insertRule);
      assert.calledWithExactly(sheetStub.insertRule, "DUMMY#CSS.SELECTOR {}");
    });
  });
});
