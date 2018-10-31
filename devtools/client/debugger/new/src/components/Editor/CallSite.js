/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { Component } from "react";

import { markText, toEditorRange } from "../../utils/editor";
import "./CallSite.css";

type MarkerType = {
  clear: Function
};

type Props = {
  callSite: Object,
  editor: Object,
  source: Object,
  breakpoint: Object,
  showCallSite: boolean
};

export default class CallSite extends Component<Props> {
  addCallSite: Function;
  marker: ?MarkerType;

  constructor() {
    super();

    this.marker = undefined;
  }

  addCallSite = (nextProps: ?Props) => {
    const { editor, callSite, breakpoint, source } = nextProps || this.props;
    const className = !breakpoint ? "call-site" : "call-site-bp";
    const sourceId = source.id;
    const editorRange = toEditorRange(sourceId, callSite.location);
    this.marker = markText(editor, className, editorRange);
  };

  clearCallSite = () => {
    if (this.marker) {
      this.marker.clear();
      this.marker = null;
    }
  };

  shouldComponentUpdate(nextProps: Props) {
    return this.props.editor !== nextProps.editor;
  }

  componentDidMount() {
    const { breakpoint, showCallSite } = this.props;

    if (!breakpoint && !showCallSite) {
      return;
    }

    this.addCallSite();
  }

  componentWillReceiveProps(nextProps: Props) {
    const { breakpoint, showCallSite } = this.props;

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
