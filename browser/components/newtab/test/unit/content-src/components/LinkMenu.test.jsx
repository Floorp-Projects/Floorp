import { ContextMenu } from "content-src/components/ContextMenu/ContextMenu";
import { _LinkMenu as LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";
import { shallow } from "enzyme";

describe("<LinkMenu>", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "" }}
        options={["CheckPinTopSite", "CheckBookmark", "OpenInNewWindow"]}
        dispatch={() => {}}
      />
    );
  });
  it("should render a ContextMenu element", () => {
    assert.ok(wrapper.find(ContextMenu).exists());
  });
  it("should pass onUpdate, and options to ContextMenu", () => {
    assert.ok(wrapper.find(ContextMenu).exists());
    const contextMenuProps = wrapper.find(ContextMenu).props();
    ["onUpdate", "options"].forEach(prop =>
      assert.property(contextMenuProps, prop)
    );
  });
  it("should give ContextMenu the correct tabbable options length for a11y", () => {
    const { options } = wrapper.find(ContextMenu).props();
    const [firstItem] = options;
    const lastItem = options[options.length - 1];

    // first item should have {first: true}
    assert.isTrue(firstItem.first);
    assert.ok(!firstItem.last);

    // last item should have {last: true}
    assert.isTrue(lastItem.last);
    assert.ok(!lastItem.first);

    // middle items should have neither
    for (let i = 1; i < options.length - 1; i++) {
      assert.ok(!options[i].first && !options[i].last);
    }
  });
  it("should show the correct options for default sites", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", isDefault: true }}
        options={["CheckBookmark"]}
        source={"TOP_SITES"}
        isPrivateBrowsingEnabled={true}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    let i = 0;
    assert.propertyVal(options[i++], "id", "newtab-menu-pin");
    assert.propertyVal(options[i++], "id", "newtab-menu-edit-topsites");
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(options[i++], "id", "newtab-menu-open-new-window");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-menu-open-new-private-window"
    );
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(options[i++], "id", "newtab-menu-dismiss");
    assert.propertyVal(options, "length", i);
    // Double check that delete options are not included for default top sites
    options
      .filter(o => o.type !== "separator")
      .forEach(o => {
        assert.notInclude(["newtab-menu-delete-history"], o.id);
      });
  });
  it("should show Unpin option for a pinned site if CheckPinTopSite in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", isPinned: true }}
        source={"TOP_SITES"}
        options={["CheckPinTopSite"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(options.find(o => o.id && o.id === "newtab-menu-unpin"));
  });
  it("should show Pin option for an unpinned site if CheckPinTopSite in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", isPinned: false }}
        source={"TOP_SITES"}
        options={["CheckPinTopSite"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(options.find(o => o.id && o.id === "newtab-menu-pin"));
  });
  it("should show Unbookmark option for a bookmarked site if CheckBookmark in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", bookmarkGuid: 1234 }}
        source={"TOP_SITES"}
        options={["CheckBookmark"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-remove-bookmark")
    );
  });
  it("should show Bookmark option for an unbookmarked site if CheckBookmark in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", bookmarkGuid: 0 }}
        source={"TOP_SITES"}
        options={["CheckBookmark"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-bookmark")
    );
  });
  it("should show Save to Pocket option for an unsaved Pocket item if CheckSavedToPocket in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", bookmarkGuid: 0 }}
        source={"HIGHLIGHTS"}
        options={["CheckSavedToPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-save-to-pocket")
    );
  });
  it("should show Delete from Pocket option for a saved Pocket item if CheckSavedToPocket in options list", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", pocket_id: 1234 }}
        source={"HIGHLIGHTS"}
        options={["CheckSavedToPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-delete-pocket")
    );
  });
  it("should show Archive from Pocket option for a saved Pocket item if CheckBookmarkOrArchive", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", pocket_id: 1234 }}
        source={"HIGHLIGHTS"}
        options={["CheckBookmarkOrArchive"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-archive-pocket")
    );
  });
  it("should show Bookmark option for an unbookmarked site if CheckBookmarkOrArchive in options list and no pocket_id", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "" }}
        source={"HIGHLIGHTS"}
        options={["CheckBookmarkOrArchive"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-bookmark")
    );
  });
  it("should show Unbookmark option for a bookmarked site if CheckBookmarkOrArchive in options list and no pocket_id", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", bookmarkGuid: 1234 }}
        source={"HIGHLIGHTS"}
        options={["CheckBookmarkOrArchive"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-remove-bookmark")
    );
  });
  it("should show Archive from Pocket option for a saved Pocket item if CheckArchiveFromPocket", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", pocket_id: 1234 }}
        source={"TOP_STORIES"}
        options={["CheckArchiveFromPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-archive-pocket")
    );
  });
  it("should show empty from no Pocket option for no saved Pocket item if CheckArchiveFromPocket", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "" }}
        source={"TOP_STORIES"}
        options={["CheckArchiveFromPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isUndefined(
      options.find(o => o.id && o.id === "newtab-menu-archive-pocket")
    );
  });
  it("should show Delete from Pocket option for a saved Pocket item if CheckDeleteFromPocket", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", pocket_id: 1234 }}
        source={"TOP_STORIES"}
        options={["CheckDeleteFromPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-delete-pocket")
    );
  });
  it("should show empty from Pocket option for no saved Pocket item if CheckDeleteFromPocket", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "" }}
        source={"TOP_STORIES"}
        options={["CheckDeleteFromPocket"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isUndefined(
      options.find(o => o.id && o.id === "newtab-menu-archive-pocket")
    );
  });
  it("should show Open File option for a downloaded item", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download", path: "foo" }}
        source={"HIGHLIGHTS"}
        options={["OpenFile"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-open-file")
    );
  });
  it("should show Show File option for a downloaded item on a default platform", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download", path: "foo" }}
        source={"HIGHLIGHTS"}
        options={["ShowFile"]}
        platform={"default"}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-show-file")
    );
  });
  it("should show Copy Downlad Link option for a downloaded item when CopyDownloadLink", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download" }}
        source={"HIGHLIGHTS"}
        options={["CopyDownloadLink"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-copy-download-link")
    );
  });
  it("should show Go To Download Page option for a downloaded item when GoToDownloadPage", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download", referrer: "foo" }}
        source={"HIGHLIGHTS"}
        options={["GoToDownloadPage"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-go-to-download-page")
    );
    assert.isFalse(options[0].disabled);
  });
  it("should show Go To Download Page option as disabled for a downloaded item when GoToDownloadPage if no referrer exists", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download", referrer: null }}
        source={"HIGHLIGHTS"}
        options={["GoToDownloadPage"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-go-to-download-page")
    );
    assert.isTrue(options[0].disabled);
  });
  it("should show Remove Download Link option for a downloaded item when RemoveDownload", () => {
    wrapper = shallow(
      <LinkMenu
        site={{ url: "", type: "download" }}
        source={"HIGHLIGHTS"}
        options={["RemoveDownload"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-menu-remove-download")
    );
  });
  it("should show Edit option", () => {
    const props = { url: "foo", label: "label" };
    const index = 5;
    wrapper = shallow(
      <LinkMenu
        site={props}
        index={5}
        source={"TOP_SITES"}
        options={["EditTopSite"]}
        dispatch={() => {}}
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    const option = options.find(
      o => o.id && o.id === "newtab-menu-edit-topsites"
    );
    assert.isDefined(option);
    assert.equal(option.action.data.index, index);
  });
  describe(".onClick", () => {
    const FAKE_EVENT = {};
    const FAKE_INDEX = 3;
    const FAKE_SOURCE = "TOP_SITES";
    const FAKE_SITE = {
      bookmarkGuid: 1234,
      hostname: "foo",
      path: "foo",
      pocket_id: "1234",
      referrer: "https://foo.com/ref",
      title: "bar",
      type: "bookmark",
      typedBonus: true,
      url: "https://foo.com",
      sponsored_tile_id: 12345,
    };
    const dispatch = sinon.stub();
    const propOptions = [
      "ShowFile",
      "CopyDownloadLink",
      "GoToDownloadPage",
      "RemoveDownload",
      "Separator",
      "ShowPrivacyInfo",
      "RemoveBookmark",
      "AddBookmark",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "BlockUrl",
      "DeleteUrl",
      "PinTopSite",
      "UnpinTopSite",
      "SaveToPocket",
      "DeleteFromPocket",
      "ArchiveFromPocket",
      "WebExtDismiss",
    ];
    const expectedActionData = {
      "newtab-menu-remove-bookmark": FAKE_SITE.bookmarkGuid,
      "newtab-menu-bookmark": {
        url: FAKE_SITE.url,
        title: FAKE_SITE.title,
        type: FAKE_SITE.type,
      },
      "newtab-menu-open-new-window": {
        url: FAKE_SITE.url,
        referrer: FAKE_SITE.referrer,
        typedBonus: FAKE_SITE.typedBonus,
        sponsored_tile_id: FAKE_SITE.sponsored_tile_id,
      },
      "newtab-menu-open-new-private-window": {
        url: FAKE_SITE.url,
        referrer: FAKE_SITE.referrer,
      },
      "newtab-menu-dismiss": [
        {
          url: FAKE_SITE.url,
          pocket_id: FAKE_SITE.pocket_id,
          isSponsoredTopSite: undefined,
        },
      ],
      menu_action_webext_dismiss: {
        source: "TOP_SITES",
        url: FAKE_SITE.url,
        action_position: 3,
      },
      "newtab-menu-delete-history": {
        url: FAKE_SITE.url,
        pocket_id: FAKE_SITE.pocket_id,
        forceBlock: FAKE_SITE.bookmarkGuid,
      },
      "newtab-menu-pin": { site: FAKE_SITE, index: FAKE_INDEX },
      "newtab-menu-unpin": { site: { url: FAKE_SITE.url } },
      "newtab-menu-save-to-pocket": {
        site: { url: FAKE_SITE.url, title: FAKE_SITE.title },
      },
      "newtab-menu-delete-pocket": { pocket_id: "1234" },
      "newtab-menu-archive-pocket": { pocket_id: "1234" },
      "newtab-menu-show-file": { url: FAKE_SITE.url },
      "newtab-menu-copy-download-link": { url: FAKE_SITE.url },
      "newtab-menu-go-to-download-page": { url: FAKE_SITE.referrer },
      "newtab-menu-remove-download": { url: FAKE_SITE.url },
    };
    const { options } = shallow(
      <LinkMenu
        site={FAKE_SITE}
        siteInfo={{ value: { card_type: FAKE_SITE.type } }}
        dispatch={dispatch}
        index={FAKE_INDEX}
        isPrivateBrowsingEnabled={true}
        platform={"default"}
        options={propOptions}
        source={FAKE_SOURCE}
        shouldSendImpressionStats={true}
      />
    )
      .find(ContextMenu)
      .props();
    afterEach(() => dispatch.reset());
    options
      .filter(o => o.type !== "separator")
      .forEach(option => {
        it(`should fire a ${option.action.type} action for ${option.id} with the expected data`, () => {
          option.onClick(FAKE_EVENT);

          if (option.impression && option.userEvent) {
            assert.calledThrice(dispatch);
          } else if (option.impression || option.userEvent) {
            assert.calledTwice(dispatch);
          } else {
            assert.calledOnce(dispatch);
          }

          // option.action is dispatched
          assert.ok(dispatch.firstCall.calledWith(option.action));

          // option.action has correct data
          // (delete is a special case as it dispatches a nested DIALOG_OPEN-type action)
          // in the case of this FAKE_SITE, we send a bookmarkGuid therefore we also want
          // to block this if we delete it
          if (option.id === "newtab-menu-delete-history") {
            assert.deepEqual(
              option.action.data.onConfirm[0].data,
              expectedActionData[option.id]
            );
            // Test UserEvent send correct meta about item deleted
            assert.propertyVal(
              option.action.data.onConfirm[1].data,
              "action_position",
              FAKE_INDEX
            );
            assert.propertyVal(
              option.action.data.onConfirm[1].data,
              "source",
              FAKE_SOURCE
            );
          } else {
            assert.deepEqual(option.action.data, expectedActionData[option.id]);
          }
        });
        it(`should fire a UserEvent action for ${option.id} if configured`, () => {
          if (option.userEvent) {
            option.onClick(FAKE_EVENT);
            const [action] = dispatch.secondCall.args;
            assert.isUserEventAction(action);
            assert.propertyVal(action.data, "source", FAKE_SOURCE);
            assert.propertyVal(action.data, "action_position", FAKE_INDEX);
            assert.propertyVal(action.data.value, "card_type", FAKE_SITE.type);
          }
        });
        it(`should send impression stats for ${option.id}`, () => {
          if (option.impression) {
            option.onClick(FAKE_EVENT);
            const [action] = dispatch.thirdCall.args;
            assert.deepEqual(action, option.impression);
          }
        });
      });
    it(`should not send impression stats if not configured`, () => {
      const fakeOptions = shallow(
        <LinkMenu
          site={FAKE_SITE}
          dispatch={dispatch}
          index={FAKE_INDEX}
          options={propOptions}
          source={FAKE_SOURCE}
          shouldSendImpressionStats={false}
        />
      )
        .find(ContextMenu)
        .props().options;

      fakeOptions
        .filter(o => o.type !== "separator")
        .forEach(option => {
          if (option.impression) {
            option.onClick(FAKE_EVENT);
            assert.calledTwice(dispatch);
            assert.notEqual(dispatch.firstCall.args[0], option.impression);
            assert.notEqual(dispatch.secondCall.args[0], option.impression);
            dispatch.reset();
          }
        });
    });
    it(`should pin a SPOC with all of the site details sent`, () => {
      const pinSpocTopSite = "PinTopSite";
      const { options: spocOptions } = shallow(
        <LinkMenu
          site={FAKE_SITE}
          siteInfo={{ value: { card_type: FAKE_SITE.type } }}
          dispatch={dispatch}
          index={FAKE_INDEX}
          isPrivateBrowsingEnabled={true}
          platform={"default"}
          options={[pinSpocTopSite]}
          source={FAKE_SOURCE}
          shouldSendImpressionStats={true}
        />
      )
        .find(ContextMenu)
        .props();

      const [pinSpocOption] = spocOptions;
      pinSpocOption.onClick(FAKE_EVENT);

      if (pinSpocOption.impression && pinSpocOption.userEvent) {
        assert.calledThrice(dispatch);
      } else if (pinSpocOption.impression || pinSpocOption.userEvent) {
        assert.calledTwice(dispatch);
      } else {
        assert.calledOnce(dispatch);
      }

      // option.action is dispatched
      assert.ok(dispatch.firstCall.calledWith(pinSpocOption.action));

      assert.deepEqual(pinSpocOption.action.data, {
        site: FAKE_SITE,
        index: FAKE_INDEX,
      });
    });
  });
});
