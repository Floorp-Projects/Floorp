/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("../utils/l10n");

loader.lazyRequireGetter(this, "HarMenuUtils",
  "devtools/client/netmonitor/src/har/har-menu-utils", true);

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
    this.onDragExit = this.onDragExit.bind(this);
    this.onDrop = this.onDrop.bind(this);
  }

  // Drag Events

  onDragEnter(event) {
    event.preventDefault();
    startDragging(findDOMNode(this));
  }

  onDragExit(event) {
    event.preventDefault();
    stopDragging(findDOMNode(this));
  }

  onDragOver(event) {
    event.preventDefault();
    event.dataTransfer.dropEffect = "copy";
  }

  onDrop(event) {
    event.preventDefault();
    stopDragging(findDOMNode(this));

    let files = event.dataTransfer.files;
    if (!files) {
      return;
    }

    let {
      actions,
      openSplitConsole,
    } = this.props;

    // Import only the first dragged file for now
    // See also:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1438792
    if (files.length) {
      let file = files[0];
      readFile(file).then(har => {
        if (har) {
          HarMenuUtils.appendPreview(har, actions, openSplitConsole);
        }
      });
    }
  }

  // Rendering

  render() {
    return (
      div({
        onDragEnter: this.onDragEnter,
        onDragOver: this.onDragOver,
        onDragExit: this.onDragExit,
        onDrop: this.onDrop},
        this.props.children,
        div({className: "dropHarFiles"},
          div({}, DROP_HAR_FILES)
        )
      )
    );
  }
}

// Helpers

function readFile(file) {
  return new Promise(resolve => {
    let fileReader = new FileReader();
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
