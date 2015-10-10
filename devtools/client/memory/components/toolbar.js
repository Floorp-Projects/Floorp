const { DOM, createClass } = require("devtools/client/shared/vendor/react");

const Toolbar = module.exports = createClass({
  displayName: "toolbar",

  render() {
    let buttons = this.props.buttons;
    return (
      DOM.div({ className: "devtools-toolbar" }, ...buttons.map(spec => {
        return DOM.button(Object.assign({}, spec, {
          className: `${spec.className || "" } devtools-button`
        }));
      }))
    );
  }
});
