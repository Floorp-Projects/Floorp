/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { PureComponent } = require("devtools/client/shared/vendor/react");
const { div, input, select, option } = require("devtools/client/shared/vendor/react-dom-factories");
const { withCommonPathPrefixRemoved } = require("devtools/client/performance-new/utils");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// A list of directories with add and remove buttons.
// Looks like this:
//
// +---------------------------------------------+
// | code/obj-m-android-opt                      |
// | code/obj-m-android-debug                    |
// | test/obj-m-test                             |
// |                                             |
// +---------------------------------------------+
//
//  [+] [-]

class DirectoryPicker extends PureComponent {
  static get propTypes() {
    return {
      dirs: PropTypes.array.isRequired,
      onAdd: PropTypes.func.isRequired,
      onRemove: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this._listBox = null;
    this._takeListBoxRef = this._takeListBoxRef.bind(this);
    this._handleAddButtonClick = this._handleAddButtonClick.bind(this);
    this._handleRemoveButtonClick = this._handleRemoveButtonClick.bind(this);
  }

  _takeListBoxRef(element) {
    this._listBox = element;
  }

  _handleAddButtonClick() {
    this.props.onAdd();
  }

  _handleRemoveButtonClick() {
    if (this._listBox && this._listBox.selectedIndex !== -1) {
      this.props.onRemove(this._listBox.selectedIndex);
    }
  }

  render() {
    const { dirs } = this.props;
    const truncatedDirs = withCommonPathPrefixRemoved(dirs);
    return [
      select(
        {
          className: "perf-settings-dir-list",
          size: "4",
          ref: this._takeListBoxRef,
        },
        dirs.map((fullPath, i) => option(
          {
            key: fullPath,
            className: "pref-settings-dir-list-item",
            title: fullPath,
          },
          truncatedDirs[i]
        ))
      ),
      div(
        { className: "perf-settings-dir-list-button-group" },
        input({
          type: "button",
          value: "+",
          title: "Add a directory",
          onClick: this._handleAddButtonClick,
        }),
        input({
          type: "button",
          value: "-",
          title: "Remove the selected directory from the list",
          onClick: this._handleRemoveButtonClick,
        }),
      ),
    ];
  }
}

module.exports = DirectoryPicker;
