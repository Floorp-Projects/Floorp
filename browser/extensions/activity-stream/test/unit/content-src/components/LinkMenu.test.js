const React = require("react");
const {shallowWithIntl} = require("test/unit/utils");
const {_unconnected: LinkMenu} = require("content-src/components/LinkMenu/LinkMenu");
const ContextMenu = require("content-src/components/ContextMenu/ContextMenu");

describe("<LinkMenu>", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = shallowWithIntl(<LinkMenu site={{url: ""}} dispatch={() => {}} />);
  });
  it("should render a ContextMenu element", () => {
    assert.ok(wrapper.find(ContextMenu));
  });
  it("should pass visible, onUpdate, and options to ContextMenu", () => {
    assert.ok(wrapper.find(ContextMenu));
    const contextMenuProps = wrapper.find(ContextMenu).props();
    ["visible", "onUpdate", "options"].forEach(prop => assert.property(contextMenuProps, prop));
  });
  it("should give ContextMenu the correct tabbable options length for a11y", () => {
    const options = wrapper.find(ContextMenu).props().options;
    const firstItem = options[0];
    const lastItem = options[options.length - 1];
    const middleItem = options[Math.ceil(options.length / 2)];

    // first item should have {first: true}
    assert.isTrue(firstItem.first);
    assert.ok(!firstItem.last);

    // last item should have {last: true}
    assert.isTrue(lastItem.last);
    assert.ok(!lastItem.first);

    // middle items should have neither
    assert.ok(!middleItem.first);
    assert.ok(!middleItem.last);
  });
});
