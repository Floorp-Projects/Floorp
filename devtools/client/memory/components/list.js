const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

/**
 * Generic list component that takes another react component to represent
 * the children nodes as `itemComponent`, and a list of items to render
 * as that component with a click handler.
 */
const List = module.exports = createClass({
  displayName: "list",

  propTypes: {
    itemComponent: PropTypes.any.isRequired,
    onClick: PropTypes.func,
    items: PropTypes.array.isRequired,
  },

  render() {
    let { items, onClick, itemComponent: Item } = this.props;

    return (
      dom.ul({ className: "list" }, items.map((item, index) => {
        return Item({
          item, index, onClick: () => onClick(item),
        });
      }))
    );
  }
});
