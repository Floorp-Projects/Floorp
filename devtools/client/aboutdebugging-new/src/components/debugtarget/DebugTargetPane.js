/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createRef, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");

const DebugTargetList = createFactory(require("./DebugTargetList"));

const Actions = require("../../actions/index");
const Types = require("../../types/index");

/**
 * This component provides list for debug target and name area.
 */
class DebugTargetPane extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      additionalActionsComponent: PropTypes.any,
      children: PropTypes.node,
      collapsibilityKey: PropTypes.string.isRequired,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      icon: PropTypes.string.isRequired,
      isCollapsed: PropTypes.bool.isRequired,
      name: PropTypes.string.isRequired,
      targets: PropTypes.arrayOf(Types.debugTarget).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.collapsableRef = createRef();
  }

  componentDidUpdate(prevProps, prevState, snapshot) {
    if (snapshot === null) {
      return;
    }

    const el = this.collapsableRef.current;

    // Cancel existing animation which is collapsing/expanding.
    for (const animation of el.getAnimations()) {
      animation.cancel();
    }

    el.animate({ maxHeight: [`${ snapshot }px`, `${ el.clientHeight }px`] },
               { duration: 150, easing: "cubic-bezier(.07, .95, 0, 1)" });
  }

  getSnapshotBeforeUpdate(prevProps) {
    if (this.props.isCollapsed !== prevProps.isCollapsed) {
      return this.collapsableRef.current.clientHeight;
    }

    return null;
  }

  toggleCollapsibility() {
    const { collapsibilityKey, dispatch, isCollapsed } = this.props;
    dispatch(Actions.updateDebugTargetCollapsibility(collapsibilityKey, !isCollapsed));
  }

  render() {
    const {
      actionComponent,
      additionalActionsComponent,
      children,
      detailComponent,
      dispatch,
      getString,
      icon,
      isCollapsed,
      name,
      targets,
    } = this.props;

    const title = getString("about-debugging-collapse-expand-debug-targets");

    return dom.section(
      {
        className: "qa-debug-target-pane",
      },
      dom.a(
        {
          className: "undecorated-link debug-target-pane__title " +
                     "qa-debug-target-pane-title",
          title,
          onClick: e => this.toggleCollapsibility(),
        },
        dom.h2(
          { className: "main-subheading debug-target-pane__heading" },
          dom.img(
            {
              className: "main-subheading__icon",
              src: icon,
            }
          ),
          `${ name } (${ targets.length })`,
          dom.img(
            {
              className: "main-subheading__icon debug-target-pane__icon" +
                            (isCollapsed ? " debug-target-pane__icon--collapsed" : ""),
              src: "chrome://devtools/skin/images/arrow-e.svg",
            }
          ),
        )
      ),
      dom.div(
        {
          className: "debug-target-pane__collapsable qa-debug-target-pane__collapsable" +
                     (isCollapsed ? " debug-target-pane__collapsable--collapsed" : ""),
          ref: this.collapsableRef,
        },
        children,
        DebugTargetList({
          actionComponent,
          additionalActionsComponent,
          detailComponent,
          dispatch,
          isCollapsed,
          targets,
        }),
      ),
    );
  }
}

module.exports = FluentReact.withLocalization(DebugTargetPane);
