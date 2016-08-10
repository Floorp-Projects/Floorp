const React = require("react");
const InlineSVG = require("svg-inline-react");

const svg = {
  "angle-brackets": require("./angle-brackets.svg"),
  "arrow": require("./arrow.svg"),
  "blackBox": require("./blackBox.svg"),
  "breakpoint": require("./breakpoint.svg"),
  "close": require("./close.svg"),
  "disableBreakpoints": require("./disableBreakpoints.svg"),
  "domain": require("./domain.svg"),
  "file": require("./file.svg"),
  "folder": require("./folder.svg"),
  "globe": require("./globe.svg"),
  "magnifying-glass": require("./magnifying-glass.svg"),
  "pause": require("./pause.svg"),
  "pause-circle": require("./pause-circle.svg"),
  "pause-exceptions": require("./pause-exceptions.svg"),
  "prettyPrint": require("./prettyPrint.svg"),
  "resume": require("./resume.svg"),
  "settings": require("./settings.svg"),
  "stepIn": require("./stepIn.svg"),
  "stepOut": require("./stepOut.svg"),
  "stepOver": require("./stepOver.svg"),
  "subSettings": require("./subSettings.svg"),
  "worker": require("./worker.svg")
};

module.exports = function(name, props) { // eslint-disable-line
  if (!svg[name]) {
    throw new Error("Unknown SVG: " + name);
  }
  let className = props ? `${name} ${props.className}` : name;
  if (name === "subSettings") {
    className = "";
  }
  props = Object.assign({}, props, { className, src: svg[name] });
  return React.createElement(InlineSVG, props);
};
