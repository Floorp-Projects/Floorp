/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {Object} Props
 * @property {string[]} dirs
 * @property {() => void} onAdd
 * @property {(index: number) => void} onRemove
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  select,
  option,
} = require("devtools/client/shared/vendor/react-dom-factories");
const {
  withCommonPathPrefixRemoved,
} = require("devtools/client/performance-new/utils");
const Localized = createFactory(
  require("devtools/client/shared/vendor/fluent-react").Localized
);

/**
 * A list of directories with add and remove buttons.
 * Looks like this:
 *
 * +---------------------------------------------+
 * | code/obj-m-android-opt                      |
 * | code/obj-m-android-debug                    |
 * | test/obj-m-test                             |
 * |                                             |
 * +---------------------------------------------+
 *
 *  [+] [-]
 *
 * @extends {React.PureComponent<Props>}
 */
class DirectoryPicker extends PureComponent {
  /** @param {Props} props */
  constructor(props) {
    super(props);
    this._listBox = null;
    this._takeListBoxRef = this._takeListBoxRef.bind(this);
    this._handleAddButtonClick = this._handleAddButtonClick.bind(this);
    this._handleRemoveButtonClick = this._handleRemoveButtonClick.bind(this);
  }

  /**
   * @param {HTMLSelectElement} element
   */
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
          size: 4,
          ref: this._takeListBoxRef,
          key: "directory-picker-select",
        },
        dirs.map((fullPath, i) =>
          option(
            {
              key: fullPath,
              className: "pref-settings-dir-list-item",
              title: fullPath,
            },
            truncatedDirs[i]
          )
        )
      ),
      div(
        {
          className: "perf-settings-dir-list-button-group",
          key: "directory-picker-div",
        },
        button(
          {
            type: "button",
            className: `perf-photon-button perf-photon-button-default perf-button`,
            onClick: this._handleAddButtonClick,
          },
          Localized({ id: "perftools-button-add-directory" })
        ),
        button(
          {
            type: "button",
            className: `perf-photon-button perf-photon-button-default perf-button`,
            onClick: this._handleRemoveButtonClick,
          },
          Localized({ id: "perftools-button-remove-directory" })
        )
      ),
    ];
  }
}

module.exports = DirectoryPicker;
