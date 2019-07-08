/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const SidebarToggle = createFactory(
  require("devtools/client/shared/components/SidebarToggle")
);
const Tabs = createFactory(
  require("devtools/client/shared/components/tabs/Tabs").Tabs
);

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.oneOfType([PropTypes.array, PropTypes.element])
        .isRequired,
      onAfterChange: PropTypes.func,
      onAllTabsMenuClick: PropTypes.func,
      renderOnlySelected: PropTypes.bool,
      showAllTabsMenu: PropTypes.bool,
      sidebarToggleButton: PropTypes.shape({
        collapsed: PropTypes.bool.isRequired,
        collapsePaneTitle: PropTypes.string.isRequired,
        expandPaneTitle: PropTypes.string.isRequired,
        onClick: PropTypes.func.isRequired,
      }),
      activeTab: PropTypes.number,
    };
  }

  constructor(props) {
    super(props);
    this.renderSidebarToggle = this.renderSidebarToggle.bind(this);
  }

  renderSidebarToggle() {
    if (!this.props.sidebarToggleButton) {
      return null;
    }

    const {
      collapsed,
      collapsePaneTitle,
      expandPaneTitle,
      onClick,
    } = this.props.sidebarToggleButton;

    return SidebarToggle({
      collapsed,
      collapsePaneTitle,
      expandPaneTitle,
      onClick,
    });
  }

  render() {
    const { renderSidebarToggle } = this;
    const {
      children,
      onAfterChange,
      onAllTabsMenuClick,
      renderOnlySelected,
      showAllTabsMenu,
      activeTab,
    } = this.props;

    return Tabs(
      {
        onAfterChange,
        onAllTabsMenuClick,
        renderOnlySelected,
        renderSidebarToggle,
        showAllTabsMenu,
        activeTab,
      },
      children
    );
  }
}

module.exports = Sidebar;
