import {
  ContextMenu,
  ContextMenuItem,
} from "content-src/components/ContextMenu/ContextMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { mount, shallow } from "enzyme";
import React from "react";

const DEFAULT_PROPS = {
  onUpdate: () => {},
  options: [],
  tabbableOptionsLength: 0,
};

const DEFAULT_MENU_OPTIONS = [
  "MoveUp",
  "MoveDown",
  "Separator",
  "RemoveSection",
  "CheckCollapsed",
  "Separator",
  "ManageSection",
];

const FakeMenu = props => {
  return <div>{props.children}</div>;
};

describe("<ContextMenuButton>", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should call onUpdate when clicked", () => {
    const onUpdate = sandbox.spy();
    const wrapper = mount(
      <ContextMenuButton onUpdate={onUpdate}>
        <FakeMenu />
      </ContextMenuButton>
    );
    wrapper.find(".context-menu-button").simulate("click");
    assert.calledOnce(onUpdate);
  });
  it("should call onUpdate when activated with Enter", () => {
    const onUpdate = sandbox.spy();
    const wrapper = mount(
      <ContextMenuButton onUpdate={onUpdate}>
        <FakeMenu />
      </ContextMenuButton>
    );
    wrapper.find(".context-menu-button").simulate("keydown", { key: "Enter" });
    assert.calledOnce(onUpdate);
  });
  it("should call onClick", () => {
    const onClick = sandbox.spy(ContextMenuButton.prototype, "onClick");
    const wrapper = mount(
      <ContextMenuButton>
        <FakeMenu />
      </ContextMenuButton>
    );
    wrapper.find("button").simulate("click");
    assert.calledOnce(onClick);
  });
  it("should have a default keyboardAccess prop of false", () => {
    const wrapper = mount(
      <ContextMenuButton>
        <ContextMenu options={DEFAULT_MENU_OPTIONS} />
      </ContextMenuButton>
    );
    wrapper.setState({ showContextMenu: true });
    assert.equal(wrapper.find(ContextMenu).prop("keyboardAccess"), false);
  });
  it("should pass the keyboardAccess prop down to ContextMenu", () => {
    const wrapper = mount(
      <ContextMenuButton>
        <ContextMenu options={DEFAULT_MENU_OPTIONS} />
      </ContextMenuButton>
    );
    wrapper.setState({ showContextMenu: true, contextMenuKeyboard: true });
    assert.equal(wrapper.find(ContextMenu).prop("keyboardAccess"), true);
  });
  it("should call focusFirst when keyboardAccess is true", () => {
    const wrapper = mount(
      <ContextMenuButton>
        <ContextMenu options={[{ label: "item1", first: true }]} />
      </ContextMenuButton>
    );
    const focusFirst = sandbox.spy(ContextMenuItem.prototype, "focusFirst");
    wrapper.setState({ showContextMenu: true, contextMenuKeyboard: true });
    assert.calledOnce(focusFirst);
  });
});

describe("<ContextMenu>", () => {
  it("should render all the options provided", () => {
    const options = [
      { label: "item1" },
      { type: "separator" },
      { label: "item2" },
    ];
    const wrapper = shallow(
      <ContextMenu {...DEFAULT_PROPS} options={options} />
    );
    assert.lengthOf(wrapper.find(".context-menu-list").children(), 3);
  });
  it("should not add a link for a separator", () => {
    const options = [{ label: "item1" }, { type: "separator" }];
    const wrapper = shallow(
      <ContextMenu {...DEFAULT_PROPS} options={options} />
    );
    assert.lengthOf(wrapper.find(".separator"), 1);
  });
  it("should add a link for all types that are not separators", () => {
    const options = [{ label: "item1" }, { type: "separator" }];
    const wrapper = shallow(
      <ContextMenu {...DEFAULT_PROPS} options={options} />
    );
    assert.lengthOf(wrapper.find(ContextMenuItem), 1);
  });
  it("should add an icon to items that need icons", () => {
    const options = [{ label: "item1", icon: "icon1" }, { type: "separator" }];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".icon-icon1"), 1);
  });
  it("should be tabbable", () => {
    const options = [{ label: "item1", icon: "icon1" }, { type: "separator" }];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.equal(
      wrapper.find(".context-menu-item").props().role,
      "presentation"
    );
  });
  it("should call onUpdate with false when an option is clicked", () => {
    const onUpdate = sinon.spy();
    const onClick = sinon.spy();
    const wrapper = mount(
      <ContextMenu
        {...DEFAULT_PROPS}
        onUpdate={onUpdate}
        options={[{ label: "item1", onClick }]}
      />
    );
    wrapper.find(".context-menu-item button").simulate("click");
    assert.calledOnce(onUpdate);
    assert.calledOnce(onClick);
  });
  it("should not have disabled className by default", () => {
    const options = [{ label: "item1", icon: "icon1" }, { type: "separator" }];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-item a.disabled"), 0);
  });
  it("should add disabled className to any disabled options", () => {
    const options = [
      { label: "item1", icon: "icon1", disabled: true },
      { type: "separator" },
    ];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-item button.disabled"), 1);
  });
  it("should have the context-menu-item class", () => {
    const options = [{ label: "item1", icon: "icon1" }];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-item"), 1);
  });
  it("should call onClick when onKeyDown is called with Enter", () => {
    const onClick = sinon.spy();
    const wrapper = mount(
      <ContextMenu {...DEFAULT_PROPS} options={[{ label: "item1", onClick }]} />
    );
    wrapper
      .find(".context-menu-item button")
      .simulate("keydown", { key: "Enter" });
    assert.calledOnce(onClick);
  });
  it("should call focusSibling when onKeyDown is called with ArrowUp", () => {
    const wrapper = mount(
      <ContextMenu {...DEFAULT_PROPS} options={[{ label: "item1" }]} />
    );
    const focusSibling = sinon.stub(
      wrapper.find(ContextMenuItem).instance(),
      "focusSibling"
    );
    wrapper
      .find(".context-menu-item button")
      .simulate("keydown", { key: "ArrowUp" });
    assert.calledOnce(focusSibling);
  });
  it("should call focusSibling when onKeyDown is called with ArrowDown", () => {
    const wrapper = mount(
      <ContextMenu {...DEFAULT_PROPS} options={[{ label: "item1" }]} />
    );
    const focusSibling = sinon.stub(
      wrapper.find(ContextMenuItem).instance(),
      "focusSibling"
    );
    wrapper
      .find(".context-menu-item button")
      .simulate("keydown", { key: "ArrowDown" });
    assert.calledOnce(focusSibling);
  });
});
