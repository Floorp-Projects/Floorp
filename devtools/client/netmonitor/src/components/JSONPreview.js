/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

// Components
const TreeView = createFactory(
  require("devtools/client/shared/components/tree/TreeView")
);
loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

/**
 * Shows JSON in a custom tree format.
 */
class JSONPreview extends Component {
  static get propTypes() {
    return {
      // Custom value renderer
      renderValue: PropTypes.func,
      cropLimit: PropTypes.number,
    };
  }

  constructor(props) {
    super(props);

    this.renderValueWithRep = this.renderValueWithRep.bind(this);
  }

  renderValueWithRep(props) {
    const { member } = props;

    // Hide strings with following conditions
    // 1. this row is a togglable section and content is object ('cause it shouldn't hide
    //    when string or number)
    // 2. the `value` object has a `value` property, only happened in Cookies panel
    // Put 2 here to not dup this method
    if (
      (member.level === 0 && member.type === "object") ||
      (typeof member.value === "object" && member.value?.value)
    ) {
      return null;
    }

    return Rep(
      Object.assign(props, {
        // FIXME: A workaround for the issue in StringRep
        // Force StringRep to crop the text every time
        member: Object.assign({}, member, { open: false }),
        mode: MODE.LONG,
        cropLimit: this.props.cropLimit,
        noGrip: true,
      })
    );
  }

  render() {
    return dom.div(
      {
        className: "tree-container",
      },
      TreeView({
        ...this.props,
        renderValue: this.props.renderValue || this.renderValueWithRep,
      })
    );
  }
}

module.exports = JSONPreview;
