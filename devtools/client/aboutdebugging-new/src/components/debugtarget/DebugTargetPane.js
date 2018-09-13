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
      {},
      dom.h2(
        {},
        dom.a(
          {
            className: "debug-target-pane__title js-debug-target-pane-title" +
                       (isCollapsed ? " debug-target-pane__title--collapsed" : ""),
            href: "#",
            onClick: e => this.toggleCollapsibility(),
          },
          name,
          isCollapsed ? dom.span({}, `(${ targets.length })`) : null,
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
