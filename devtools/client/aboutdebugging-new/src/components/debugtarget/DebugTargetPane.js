/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const DebugTargetList = createFactory(require("./DebugTargetList"));

const Actions = require("../../actions/index");

/**
 * This component provides list for debug target and name area.
 */
class DebugTargetPane extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      collapsibilityKey: PropTypes.string.isRequired,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      isCollapsed: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      targets: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  toggleCollapsibility() {
    const { collapsibilityKey, dispatch, isCollapsed } = this.props;
    dispatch(Actions.updateDebugTargetCollapsibility(collapsibilityKey, !isCollapsed));
  }

  render() {
    const {
      actionComponent,
      detailComponent,
      dispatch,
      isCollapsed,
      name,
      targets,
    } = this.props;

    return dom.section(
      {
        className: "js-debug-target-pane",
      },
      dom.a(
        {
          className: "undecorated-link js-debug-target-pane-title",
          href: "#",
          onClick: e => this.toggleCollapsibility(),
        },
        dom.h2(
          { className: "main-subheading" },
          dom.img(
            {
              className: "main-subheading__icon debug-target-pane__icon" +
                         (isCollapsed ? " debug-target-pane__icon--collapsed" : ""),
              src: "chrome://devtools/skin/images/aboutdebugging-collapse-icon.svg",
            }
          ),
          name + (isCollapsed ? ` (${ targets.length })` : ""),
        )
      ),
      DebugTargetList({
        actionComponent,
        detailComponent,
        dispatch,
        isCollapsed,
        targets,
      }),
    );
  }
}

module.exports = DebugTargetPane;
