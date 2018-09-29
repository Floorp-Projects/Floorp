/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

loader.lazyRequireGetter(this, "translateNodeFrontToGrip", "devtools/client/inspector/shared/utils", true);

const Types = require("../types");

class FlexItem extends PureComponent {
  static get propTypes() {
    return {
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
      onToggleFlexItemShown: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      flexItem,
      onToggleFlexItemShown,
    } = this.props;
    const { nodeFront } = flexItem;

    return (
      dom.li({},
        dom.button(
          {
            className: "devtools-button devtools-monospace",
            onClick: () => onToggleFlexItemShown(nodeFront),
          },
          Rep({
            defaultRep: Rep.ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(nodeFront),
          })
        )
      )
    );
  }
}

module.exports = FlexItem;
