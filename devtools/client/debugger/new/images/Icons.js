const React = require("react");
const InlineSVG = require("svg-inline-react");
const { DOM: dom } = React;

const DomainIcon = props => {
  return dom.span(
    props,
    React.createElement(InlineSVG, {
      src: require("./domain.svg")
    })
  );
};

const FileIcon = props => {
  return dom.span(
    props,
    React.createElement(InlineSVG, {
      src: require("./file.svg")
    })
  );
};

const FolderIcon = props => {
  return dom.span(
    props,
    React.createElement(InlineSVG, {
      src: require("./folder.svg")
    })
  );
};

const WorkerIcon = props => {
  return dom.span(
    props,
    React.createElement(InlineSVG, {
      src: require("./worker.svg")
    })
  );
};

module.exports = {
  DomainIcon,
  FileIcon,
  FolderIcon,
  WorkerIcon
};
