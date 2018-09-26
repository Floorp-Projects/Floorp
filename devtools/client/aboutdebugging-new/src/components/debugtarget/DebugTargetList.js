/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, createRef, PureComponent } =
  require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const DebugTargetItem = createFactory(require("./DebugTargetItem"));

/**
 * This component displays list of debug target.
 */
class DebugTargetList extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      isCollapsed: PropTypes.bool.isRequired,
      targets: PropTypes.arrayOf(PropTypes.object).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.listRef = createRef();
  }

  componentDidUpdate(prevProps, prevState, snapshot) {
    if (snapshot === null) {
      return;
    }

    const list = this.listRef.current;
    list.animate({ maxHeight: [`${ snapshot }px`, `${ list.clientHeight }px`] },
                 { duration: 150, easing: "cubic-bezier(.07, .95, 0, 1)" });
  }

  getSnapshotBeforeUpdate(prevProps) {
    if (this.props.isCollapsed !== prevProps.isCollapsed) {
      return this.listRef.current.clientHeight;
    }

    return null;
  }

  renderEmptyList() {
    return Localized(
      {
        id: "about-debugging-debug-target-list-empty"
      },
      dom.span(
        {
          className: "js-debug-target-list-empty"
        },
        "Nothing yet."
      )
    );
  }

  render() {
    const {
      actionComponent,
      detailComponent,
      dispatch,
      isCollapsed,
      targets,
    } = this.props;

    return dom.ul(
      {
        className: "debug-target-list js-debug-target-list" +
                   (isCollapsed ? " debug-target-list--collapsed" : ""),
        ref: this.listRef,
      },
      targets.length === 0
        ? this.renderEmptyList()
        : targets.map((target, key) =>
            DebugTargetItem({ actionComponent, detailComponent, dispatch, key, target })),
    );
  }
}

module.exports = DebugTargetList;
