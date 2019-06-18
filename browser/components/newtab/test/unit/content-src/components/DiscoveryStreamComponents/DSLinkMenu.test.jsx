import {mountWithIntl, shallowWithIntl} from "test/unit/utils";
import {_DSLinkMenu as DSLinkMenu} from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import {LinkMenu} from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

describe("<DSLinkMenu>", () => {
  let wrapper;
  let parentNode;

  describe("DS link menu actions", () => {
    beforeEach(() => {
      wrapper = mountWithIntl(<DSLinkMenu />);
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

    it("Should add active on Menu Show", () => {
      wrapper.instance().onMenuShow();
      wrapper.update();
      assert.equal(parentNode.className, "active");
    });

    it("Should add last-item to support resized window", () => {
      const fakeWindow = {scrollMaxX: "20"};
      wrapper = mountWithIntl(<DSLinkMenu windowObj={fakeWindow} />);
      parentNode = wrapper.getDOMNode().parentNode;
      wrapper.instance().onMenuShow();
      wrapper.update();
      assert.equal(parentNode.className, "last-item active");
    });
  });

  describe("DS context menu options", () => {
    const ValidDSLinkMenuProps = {
      site: {},
    };

    beforeEach(() => {
      wrapper = shallowWithIntl(<DSLinkMenu {...ValidDSLinkMenuProps} />);
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
