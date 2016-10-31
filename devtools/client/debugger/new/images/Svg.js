const React = require("react");
const InlineSVG = require("svg-inline-react");

const svg = {
  "angle-brackets": require("./angle-brackets.svg"),
  "arrow": require("./arrow.svg"),
  "blackBox": require("./blackBox.svg"),
  "breakpoint": require("./breakpoint.svg"),
  "close": require("./close.svg"),
  "domain": require("./domain.svg"),
  "file": require("./file.svg"),
  "folder": require("./folder.svg"),
  "globe": require("./globe.svg"),
  "magnifying-glass": require("./magnifying-glass.svg"),
  "pause": require("./pause.svg"),
  "pause-exceptions": require("./pause-exceptions.svg"),
  "plus": require("./plus.svg"),
  "prettyPrint": require("./prettyPrint.svg"),
  "resume": require("./resume.svg"),
  "settings": require("./settings.svg"),
  "stepIn": require("./stepIn.svg"),
  "stepOut": require("./stepOut.svg"),
  "stepOver": require("./stepOver.svg"),
  "subSettings": require("./subSettings.svg"),
  "toggleBreakpoints": require("./toggle-breakpoints.svg"),
  "worker": require("./worker.svg"),
  "sad-face": require("./sad-face.svg")
};

module.exports = function(name, props) { // eslint-disable-line
  if (!svg[name]) {
    throw new Error("Unknown SVG: " + name);
  }
  let className = name;
  if (props && props.className) {
    className = `${name} ${props.className}`;
  }
  if (name === "subSettings") {
    className = "";
  }
  props = Object.assign({}, props, { className, src: svg[name] });
  return React.createElement(InlineSVG, props);
};
