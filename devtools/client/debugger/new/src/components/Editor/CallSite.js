"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _editor = require("../../utils/editor/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class CallSite extends _react.Component {
  constructor() {
    super();

    this.addCallSite = nextProps => {
      const {
        editor,
        callSite,
        breakpoint,
        source
      } = nextProps || this.props;
      const className = !breakpoint ? "call-site" : "call-site-bp";
      const sourceId = source.get("id");
      const editorRange = (0, _editor.toEditorRange)(sourceId, callSite.location);
      this.marker = (0, _editor.markText)(editor, className, editorRange);
    };

    this.clearCallSite = () => {
      if (this.marker) {
        this.marker.clear();
        this.marker = null;
      }
    };

    this.marker = undefined;
  }

  shouldComponentUpdate(nextProps) {
    return this.props.editor !== nextProps.editor;
  }

  componentDidMount() {
    const {
      breakpoint,
      showCallSite
    } = this.props;

    if (!breakpoint && !showCallSite) {
      return;
    }

    this.addCallSite();
  }

  componentWillReceiveProps(nextProps) {
    const {
      breakpoint,
      showCallSite
    } = this.props;

    if (nextProps.breakpoint !== breakpoint) {
      if (this.marker) {
        this.clearCallSite();
      }

      if (nextProps.showCallSite) {
        this.addCallSite(nextProps);
      }
    }

    if (nextProps.showCallSite !== showCallSite) {
      if (nextProps.showCallSite) {
        if (!this.marker) {
          this.addCallSite();
        }
      } else if (!nextProps.breakpoint) {
        this.clearCallSite();
      }
    }
  }

  componentWillUnmount() {
    if (!this.marker) {
      return;
    }

    this.marker.clear();
  }

  render() {
    return null;
  }

}

exports.default = CallSite;