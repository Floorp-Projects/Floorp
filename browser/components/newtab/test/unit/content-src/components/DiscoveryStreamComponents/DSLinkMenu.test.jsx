import {mount, shallow} from "enzyme";
import {DSLinkMenu} from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import {LinkMenu} from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

describe("<DSLinkMenu>", () => {
  let wrapper;
  let parentNode;

  describe("DS link menu actions", () => {
    beforeEach(() => {
      wrapper = mount(<DSLinkMenu />);
      parentNode = wrapper.getDOMNode().parentNode;
    });

    afterEach(() => {
      wrapper.unmount();
    });

    it("Should remove active on Menu Update", () => {
      wrapper.setState({showContextMenu: true});
      // Add active class name to DSLinkMenu parent node
      // to simulate menu open state
      parentNode.classList.add("active");
      assert.equal(parentNode.className, "active");

      wrapper.instance().onMenuUpdate(false);
      wrapper.update();

      assert.isEmpty(parentNode.className);
      assert.isFalse(wrapper.state(["showContextMenu"]));
    });

    it("Should add active on Menu Show", async () => {
      wrapper.instance().onMenuShow();
      // Wait for next frame to allow fluent to render strings
      await new Promise(r => requestAnimationFrame(r));
      wrapper.update();
      assert.equal(parentNode.className, "active");
    });

    it("Should add last-item to support resized window", async () => {
      const fakeWindow = {scrollMaxX: "20"};
      wrapper = mount(<DSLinkMenu windowObj={fakeWindow} />);
      parentNode = wrapper.getDOMNode().parentNode;
      wrapper.instance().onMenuShow();
      // Wait for next frame to allow fluent to render strings
      await new Promise(r => requestAnimationFrame(r));
      wrapper.update();
      assert.equal(parentNode.className, "last-item active");
    });

    it("should remove .active and .last-item classes from the parent component", () => {
      const instance = wrapper.instance();
      const remove = sinon.stub();
      instance.contextMenuButtonRef = {current: {parentElement: {parentElement: {classList: {remove}}}}};
      instance.onMenuUpdate();
      assert.calledOnce(remove);
    });

    it("should add .active and .last-item classes to the parent component", async () => {
      const instance = wrapper.instance();
      const add = sinon.stub();
      instance.contextMenuButtonRef = {current: {parentElement: {parentElement: {classList: {add}}}}};
      instance.onMenuShow();
      // Wait for next frame to allow fluent to render strings
      await new Promise(r => requestAnimationFrame(r));
      assert.calledOnce(add);
    });

    it("should parse args for fluent correctly ", () => {
      const title = '"fluent"'
      wrapper = shallow(<DSLinkMenu title={title} />);

      const button = wrapper.find("button[data-l10n-id='newtab-menu-content-tooltip']");
      assert.equal(button.prop("data-l10n-args"), JSON.stringify({title}));
    });
  });

  describe("DS context menu options", () => {
    const ValidDSLinkMenuProps = {
      site: {},
    };

    beforeEach(() => {
      wrapper = shallow(<DSLinkMenu {...ValidDSLinkMenuProps} />);
    });

    it("should render a context menu button", () => {
      assert.ok(wrapper.exists());
      assert.ok(wrapper.find(".context-menu-button").exists());
    });

    it("should render LinkMenu when context menu button is clicked", () => {
      let button = wrapper.find(".context-menu-button");
      button.simulate("click", {preventDefault: () => {}});
      assert.equal(wrapper.find(LinkMenu).length, 1);
    });

    it("should pass dispatch, onUpdate, onShow, site, options, shouldSendImpressionStats, source and index to LinkMenu", () => {
      wrapper.find(".context-menu-button").simulate("click", {preventDefault: () => {}});
      const linkMenuProps = wrapper.find(LinkMenu).props();
      ["dispatch", "onUpdate", "onShow", "site", "index", "options", "source", "shouldSendImpressionStats"].forEach(prop => assert.property(linkMenuProps, prop));
    });

    it("should pass through the correct menu options to LinkMenu", () => {
      wrapper.find(".context-menu-button").simulate("click", {preventDefault: () => {}});
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options,
        ["CheckBookmarkOrArchive", "CheckSavedToPocket", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"]);
    });
  });
});
