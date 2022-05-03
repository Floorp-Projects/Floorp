import { mount, shallow } from "enzyme";
import { DSLinkMenu } from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

describe("<DSLinkMenu>", () => {
  let wrapper;

  describe("DS link menu actions", () => {
    beforeEach(() => {
      wrapper = mount(<DSLinkMenu />);
    });

    afterEach(() => {
      wrapper.unmount();
    });

    it("should parse args for fluent correctly ", () => {
      const title = '"fluent"';
      wrapper = mount(<DSLinkMenu title={title} />);

      const button = wrapper.find(
        "button[data-l10n-id='newtab-menu-content-tooltip']"
      );
      assert.equal(button.prop("data-l10n-args"), JSON.stringify({ title }));
    });
  });

  describe("DS context menu options", () => {
    const ValidDSLinkMenuProps = {
      site: {},
      pocket_button_enabled: true,
    };

    beforeEach(() => {
      wrapper = shallow(<DSLinkMenu {...ValidDSLinkMenuProps} />);
    });

    it("should render a context menu button", () => {
      assert.ok(wrapper.exists());
      assert.ok(
        wrapper.find(ContextMenuButton).exists(),
        "context menu button exists"
      );
    });

    it("should render LinkMenu when context menu button is clicked", () => {
      let button = wrapper.find(ContextMenuButton);
      button.simulate("click", { preventDefault: () => {} });
      assert.equal(wrapper.find(LinkMenu).length, 1);
    });

    it("should pass dispatch, onShow, site, options, shouldSendImpressionStats, source and index to LinkMenu", () => {
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      [
        "dispatch",
        "onShow",
        "site",
        "index",
        "options",
        "source",
        "shouldSendImpressionStats",
      ].forEach(prop => assert.property(linkMenuProps, prop));
    });

    it("should pass through the correct menu options to LinkMenu", () => {
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "CheckArchiveFromPocket",
        "CheckSavedToPocket",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
      ]);
    });

    it("should pass through the correct menu options to LinkMenu for spocs", () => {
      wrapper = shallow(
        <DSLinkMenu
          {...ValidDSLinkMenuProps}
          flightId="1234"
          showPrivacyInfo={true}
        />
      );
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "CheckArchiveFromPocket",
        "CheckSavedToPocket",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
        "ShowPrivacyInfo",
      ]);
    });

    it("should pass through the correct menu options to LinkMenu for save to Pocket button", () => {
      wrapper = shallow(
        <DSLinkMenu {...ValidDSLinkMenuProps} saveToPocketCard={true} />
      );
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "CheckArchiveFromPocket",
        "CheckDeleteFromPocket",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
      ]);
    });

    it("should pass through the correct menu options to LinkMenu if Pocket is disabled", () => {
      wrapper = shallow(
        <DSLinkMenu {...ValidDSLinkMenuProps} pocket_button_enabled={false} />
      );
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "CheckArchiveFromPocket",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
      ]);
    });
  });
});
