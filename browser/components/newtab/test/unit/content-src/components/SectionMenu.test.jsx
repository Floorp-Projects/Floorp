import { ContextMenu } from "content-src/components/ContextMenu/ContextMenu";
import React from "react";
import { _SectionMenu as SectionMenu } from "content-src/components/SectionMenu/SectionMenu";
import { shallow } from "enzyme";

const DEFAULT_PROPS = {
  name: "Section Name",
  id: "sectionId",
  source: "TOP_SITES",
  showPrefName: "showSection",
  collapsePrefName: "collapseSection",
  collapsed: false,
  onUpdate: () => {},
  visible: false,
  dispatch: () => {},
};

describe("<SectionMenu>", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} />);
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
  it("should show the correct default options", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} />);
    const { options } = wrapper.find(ContextMenu).props();
    let i = 0;
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-up");
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-down");
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-remove-section"
    );
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-collapse-section"
    );
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-manage-section"
    );
    assert.propertyVal(options, "length", i);
  });
  it("should show the correct default options for a web extension", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} isWebExtension={true} />);
    const { options } = wrapper.find(ContextMenu).props();
    let i = 0;
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-up");
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-down");
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-collapse-section"
    );
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(options[i++], "id", "newtab-section-menu-manage-webext");
    assert.propertyVal(options, "length", i);
  });
  it("should show Collapse option for an expanded section if CheckCollapsed in options list", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} collapsed={false} />);
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-section-menu-collapse-section")
    );
  });
  it("should show Expand option for a collapsed section if CheckCollapsed in options list", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} collapsed={true} />);
    const { options } = wrapper.find(ContextMenu).props();
    assert.isDefined(
      options.find(o => o.id && o.id === "newtab-section-menu-expand-section")
    );
  });
  it("should show Add Top Site option", () => {
    wrapper = shallow(
      <SectionMenu {...DEFAULT_PROPS} extraOptions={["AddTopSite"]} />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.equal(options[0].id, "newtab-section-menu-add-topsite");
  });
  it("should show Add Search Engine option", () => {
    wrapper = shallow(
      <SectionMenu {...DEFAULT_PROPS} extraOptions={["AddSearchShortcut"]} />
    );
    const { options } = wrapper.find(ContextMenu).props();
    assert.equal(options[0].id, "newtab-section-menu-add-search-engine");
  });
  it("should show Privacy Notice option if privacyNoticeURL is passed", () => {
    wrapper = shallow(
      <SectionMenu
        {...DEFAULT_PROPS}
        privacyNoticeURL="https://mozilla.org/privacy"
      />
    );
    const { options } = wrapper.find(ContextMenu).props();
    let i = 0;
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-up");
    assert.propertyVal(options[i++], "id", "newtab-section-menu-move-down");
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-remove-section"
    );
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-collapse-section"
    );
    assert.propertyVal(options[i++], "type", "separator");
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-privacy-notice"
    );
    assert.propertyVal(
      options[i++],
      "id",
      "newtab-section-menu-manage-section"
    );
    assert.propertyVal(options, "length", i);
  });
  it("should disable Move Up on first section", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} isFirst={true} />);
    const { options } = wrapper.find(ContextMenu).props();
    assert.ok(options[0].disabled);
  });
  it("should disable Move Down on last section", () => {
    wrapper = shallow(<SectionMenu {...DEFAULT_PROPS} isLast={true} />);
    const { options } = wrapper.find(ContextMenu).props();
    assert.ok(options[1].disabled);
  });
  describe(".onClick", () => {
    const dispatch = sinon.stub();
    const propOptions = [
      "Separator",
      "MoveUp",
      "MoveDown",
      "RemoveSection",
      "CollapseSection",
      "ExpandSection",
      "ManageSection",
      "AddTopSite",
      "PrivacyNotice",
    ];
    const expectedActionData = {
      "newtab-section-menu-move-up": { id: "sectionId", direction: -1 },
      "newtab-section-menu-move-down": { id: "sectionId", direction: +1 },
      "newtab-section-menu-remove-section": {
        name: "showSection",
        value: false,
      },
      "newtab-section-menu-collapse-section": {
        id: DEFAULT_PROPS.id,
        value: { collapsed: true },
      },
      "newtab-section-menu-expand-section": {
        id: DEFAULT_PROPS.id,
        value: { collapsed: false },
      },
      "newtab-section-menu-manage-section": undefined,
      "newtab-section-menu-add-topsite": { index: -1 },
      "newtab-section-menu-privacy-notice": {
        url: DEFAULT_PROPS.privacyNoticeURL,
      },
    };
    const { options } = shallow(
      <SectionMenu
        {...DEFAULT_PROPS}
        dispatch={dispatch}
        options={propOptions}
      />
    )
      .find(ContextMenu)
      .props();
    afterEach(() => dispatch.reset());
    options
      .filter(o => o.type !== "separator")
      .forEach(option => {
        it(`should fire a ${option.action.type} action for ${
          option.id
        } with the expected data`, () => {
          option.onClick();

          if (option.userEvent && option.action) {
            assert.calledTwice(dispatch);
          } else if (option.userEvent || option.action) {
            assert.calledOnce(dispatch);
          } else {
            assert.notCalled(dispatch);
          }

          // option.action is dispatched
          assert.ok(dispatch.firstCall.calledWith(option.action));
          assert.deepEqual(option.action.data, expectedActionData[option.id]);
        });
        it(`should fire a UserEvent action for ${
          option.id
        } if configured`, () => {
          if (option.userEvent) {
            option.onClick();
            const [action] = dispatch.secondCall.args;
            assert.isUserEventAction(action);
            assert.propertyVal(action.data, "source", DEFAULT_PROPS.source);
          }
        });
      });
  });
});
