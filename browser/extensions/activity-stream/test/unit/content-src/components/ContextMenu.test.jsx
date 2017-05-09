const React = require("react");
const {shallow, mount} = require("enzyme");
const ContextMenu = require("content-src/components/ContextMenu/ContextMenu");
const DEFAULT_PROPS = {
  onUpdate: () => {},
  visible: false,
  options: [],
  tabbableOptionsLength: 0
};

describe("<ContextMenu>", () => {
  it("shoud be hidden by default", () => {
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} />);
    assert.isTrue(wrapper.find(".context-menu").props().hidden);
  });
  it("should be visible if props.visible is true", () => {
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} visible={true} />);
    assert.isFalse(wrapper.find(".context-menu").props().hidden);
  });
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
    assert.lengthOf(wrapper.find(".context-menu-item"), 1);
  });
  it("should add an icon to items that need icons", () => {
    const options = [{label: "item1", icon: "icon1"}, {type: "separator"}];
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.lengthOf(wrapper.find(".icon-icon1"), 1);
  });
  it("should be tabbable", () => {
    const options = [{label: "item1", icon: "icon1"}, {type: "separator"}];
    const wrapper = shallow(<ContextMenu {...DEFAULT_PROPS} options={options} />);
    assert.equal(wrapper.find(".context-menu-item").props().role, "menuitem");
  });
  it("should call onUpdate with false when an option is clicked", () => {
    const wrapper = mount(<ContextMenu {...DEFAULT_PROPS} onUpdate={sinon.spy()} options={[{label: "item1"}]} />);
    wrapper.find(".context-menu-item").simulate("click");
    const onUpdate = wrapper.prop("onUpdate");
    assert.calledOnce(onUpdate);
  });
});
