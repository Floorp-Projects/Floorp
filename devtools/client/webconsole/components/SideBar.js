/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createElement,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

loader.lazyRequireGetter(
  this,
  "dom",
  "devtools/client/shared/vendor/react-dom-factories"
);
loader.lazyRequireGetter(
  this,
  "getObjectInspector",
  "devtools/client/webconsole/utils/object-inspector",
  true
);
loader.lazyRequireGetter(
  this,
  "actions",
  "devtools/client/webconsole/actions/index"
);
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "SplitBox",
  "devtools/client/shared/components/splitter/SplitBox"
);
loader.lazyRequireGetter(
  this,
  "reps",
  "devtools/client/shared/components/reps/reps"
);
loader.lazyRequireGetter(
  this,
  "l10n",
  "devtools/client/webconsole/utils/messages",
  true
);

class SideBar extends Component {
  static get propTypes() {
    return {
      serviceContainer: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      visible: PropTypes.bool,
      grip: PropTypes.object,
      onResized: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.onClickSidebarClose = this.onClickSidebarClose.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    const { grip, visible } = nextProps;

    return visible !== this.props.visible || grip !== this.props.grip;
  }

  onClickSidebarClose() {
    this.props.dispatch(actions.sidebarClose());
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    const { grip, serviceContainer, onResized } = this.props;

    const objectInspector = getObjectInspector(grip, serviceContainer, {
      autoExpandDepth: 1,
      mode: reps.MODE.SHORT,
      autoFocusRoot: true,
    });

    const endPanel = dom.aside(
      {
        className: "sidebar-wrapper",
      },
      dom.header(
        {
          className: "devtools-toolbar webconsole-sidebar-toolbar",
        },
        dom.button({
          className: "devtools-button sidebar-close-button",
          title: l10n.getStr("webconsole.closeSidebarButton.tooltip"),
          onClick: this.onClickSidebarClose,
        })
      ),
      dom.aside(
        {
          className: "sidebar-contents",
        },
        objectInspector
      )
    );

    return createElement(SplitBox, {
      className: "sidebar",
      endPanel,
      endPanelControl: true,
      initialSize: "200px",
      minSize: "100px",
      vert: true,
      onControlledPanelResized: onResized,
    });
  }
}

function mapStateToProps(state, props) {
  return {
    grip: state.ui.gripInSidebar,
  };
}

module.exports = connect(mapStateToProps)(SideBar);
