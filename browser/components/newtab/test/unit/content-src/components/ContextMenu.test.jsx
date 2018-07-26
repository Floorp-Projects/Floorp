import {ContextMenu, ContextMenuItem} from "content-src/components/ContextMenu/ContextMenu";
import {mount, shallow} from "enzyme";
import React from "react";

const DEFAULT_PROPS = {
  onUpdate: () => {},
  options: [],
  tabbableOptionsLength: 0
};

describe("<ContextMenu>", () => {
  it("should render all the options provided", () => {
    const options = [{label: "item1"}, {type: "separator"}, {label: "item2"}];
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-list").children(), 3);
  });
  it("should not add a link for a separator", () => {
    const options = [{label: "item1"}, {type: "separator"}];
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".separator"), 1);
  });
  it("should add a link for all types that are not separators", () => {
    const options = [{label: "item1"}, {type: "separator"}];
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(ContextMenuItem), 1);
  });
  it("should add an icon to items that need icons", () => {
    const options = [{label: "item1", icon: "icon1"}, {type: "separator"}];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".icon-icon1"), 1);
  });
  it("should be tabbable", () => {
    const options = [{label: "item1", icon: "icon1"}, {type: "separator"}];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.equal(wrapper.find(".context-menu-item").props().role, "menuitem");
  });
  it("should call onUpdate with false when an option is clicked", () => {
    const onUpdate = sinon.spy();
    const onClick = sinon.spy();
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} onUpdate={onUpdate} options={[{label: "item1", onClick}]} />);
    wrapper.find(".context-menu-item a").simulate("click");
    assert.calledOnce(onUpdate);
    assert.calledOnce(onClick);
  });
  it("should not have disabled className by default", () => {
    const options = [{label: "item1", icon: "icon1"}, {type: "separator"}];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-item a.disabled"), 0);
  });
  it("should add disabled className to any disabled options", () => {
    const options = [{label: "item1", icon: "icon1", disabled: true}, {type: "separator"}];
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".context-menu-item a.disabled"), 1);
  });
});
