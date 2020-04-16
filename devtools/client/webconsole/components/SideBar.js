/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const GridElementWidthResizer = createFactory(
  require("devtools/client/shared/components/splitter/GridElementWidthResizer")
);
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
      front: PropTypes.object,
      onResized: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.onClickSidebarClose = this.onClickSidebarClose.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    const { front } = nextProps;
    return front !== this.props.front;
  }

  onClickSidebarClose() {
    this.props.dispatch(actions.sidebarClose());
  }

  render() {
    const { front, serviceContainer } = this.props;

    const objectInspector = getObjectInspector(front, serviceContainer, {
      autoExpandDepth: 1,
      mode: reps.MODE.SHORT,
      autoFocusRoot: true,
      pathPrefix: "WebConsoleSidebar",
      customFormat: false,
    });

    return [
      dom.aside(
        {
          className: "sidebar",
          key: "sidebar",
          ref: node => {
            this.node = node;
          },
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
      ),
      GridElementWidthResizer({
        key: "resizer",
        enabled: true,
        position: "start",
        className: "sidebar-resizer",
        getControlledElementNode: () => this.node,
      }),
    ];
  }
}

function mapStateToProps(state, props) {
  return {
    front: state.ui.frontInSidebar,
  };
}

module.exports = connect(mapStateToProps)(SideBar);
