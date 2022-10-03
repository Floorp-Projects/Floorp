/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  findDOMNode,
} = require("resource://devtools/client/shared/vendor/react-dom.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

loader.lazyRequireGetter(
  this,
  "HarMenuUtils",
  "resource://devtools/client/netmonitor/src/har/har-menu-utils.js",
  true
);

const { div } = dom;

const DROP_HAR_FILES = L10N.getStr("netmonitor.label.dropHarFiles");

/**
 * Helper component responsible for handling and  importing
 * dropped *.har files.
 */
class DropHarHandler extends Component {
  static get propTypes() {
    return {
      // List of actions passed to HAR importer.
      actions: PropTypes.object.isRequired,
      // Child component.
      children: PropTypes.element.isRequired,
      // Callback for opening split console.
      openSplitConsole: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    // Drag and drop event handlers.
    this.onDragEnter = this.onDragEnter.bind(this);
    this.onDragOver = this.onDragOver.bind(this);
    this.onDragLeave = this.onDragLeave.bind(this);
    this.onDrop = this.onDrop.bind(this);
  }

  // Drag Events

  onDragEnter(event) {
    event.preventDefault();
    if (event.dataTransfer.files.length === 0) {
      return;
    }

    startDragging(findDOMNode(this));
  }

  onDragLeave(event) {
    const node = findDOMNode(this);
    if (!node.contains(event.relatedTarget)) {
      stopDragging(node);
    }
  }

  onDragOver(event) {
    event.preventDefault();
    event.dataTransfer.dropEffect = "copy";
  }

  onDrop(event) {
    event.preventDefault();
    stopDragging(findDOMNode(this));

    const { files } = event.dataTransfer;
    if (!files) {
      return;
    }

    const { actions, openSplitConsole } = this.props;

    // Import only the first dragged file for now
    // See also:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1438792
    if (files.length) {
      const file = files[0];
      readFile(file).then(har => {
        if (har) {
          HarMenuUtils.appendPreview(har, actions, openSplitConsole);
        }
      });
    }
  }

  // Rendering

  render() {
    return div(
      {
        onDragEnter: this.onDragEnter,
        onDragOver: this.onDragOver,
        onDragLeave: this.onDragLeave,
        onDrop: this.onDrop,
      },
      this.props.children,
      div({ className: "dropHarFiles" }, div({}, DROP_HAR_FILES))
    );
  }
}

// Helpers

function readFile(file) {
  return new Promise(resolve => {
    const fileReader = new FileReader();
    fileReader.onloadend = () => {
      resolve(fileReader.result);
    };
    fileReader.readAsText(file);
  });
}

function startDragging(node) {
  node.setAttribute("dragging", "true");
}

function stopDragging(node) {
  node.removeAttribute("dragging");
}

// Exports

module.exports = DropHarHandler;
