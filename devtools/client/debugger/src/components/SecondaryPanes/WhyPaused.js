/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  LocalizationProvider,
  Localized,
} = require("resource://devtools/client/shared/vendor/fluent-react.js");

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import { div, span } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";
import AccessibleImage from "../shared/AccessibleImage";
import actions from "../../actions/index";

import Reps from "devtools/client/shared/components/reps/index";
const {
  REPS: { Rep },
  MODE,
} = Reps;

import { getPauseReason } from "../../utils/pause/index";
import {
  getCurrentThread,
  getPaneCollapse,
  getPauseReason as getWhy,
} from "../../selectors/index";

class WhyPaused extends PureComponent {
  constructor(props) {
    super(props);
    this.state = { hideWhyPaused: "" };
  }

  static get propTypes() {
    return {
      delay: PropTypes.number.isRequired,
      endPanelCollapsed: PropTypes.bool.isRequired,
      highlightDomElement: PropTypes.func.isRequired,
      openElementInInspector: PropTypes.func.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
      why: PropTypes.object,
    };
  }

  componentDidUpdate() {
    const { delay } = this.props;

    if (delay) {
      setTimeout(() => {
        this.setState({ hideWhyPaused: "" });
      }, delay);
    } else {
      this.setState({ hideWhyPaused: "pane why-paused" });
    }
  }

  renderExceptionSummary(exception) {
    if (typeof exception === "string") {
      return exception;
    }

    const { preview } = exception;
    if (!preview || !preview.name || !preview.message) {
      return null;
    }

    return `${preview.name}: ${preview.message}`;
  }

  renderMessage(why) {
    const { type, exception, message } = why;

    if (type == "exception" && exception) {
      // Our types for 'Why' are too general because 'type' can be 'string'.
      // $FlowFixMe - We should have a proper discriminating union of reasons.
      const summary = this.renderExceptionSummary(exception);
      return div(
        {
          className: "message warning",
        },
        summary
      );
    }

    if (type === "mutationBreakpoint" && why.nodeGrip) {
      const { nodeGrip, ancestorGrip, action } = why;
      const {
        openElementInInspector,
        highlightDomElement,
        unHighlightDomElement,
      } = this.props;

      const targetRep = Rep({
        object: nodeGrip,
        mode: MODE.TINY,
        onDOMNodeClick: () => openElementInInspector(nodeGrip),
        onInspectIconClick: () => openElementInInspector(nodeGrip),
        onDOMNodeMouseOver: () => highlightDomElement(nodeGrip),
        onDOMNodeMouseOut: () => unHighlightDomElement(),
      });

      const ancestorRep = ancestorGrip
        ? Rep({
            object: ancestorGrip,
            mode: MODE.TINY,
            onDOMNodeClick: () => openElementInInspector(ancestorGrip),
            onInspectIconClick: () => openElementInInspector(ancestorGrip),
            onDOMNodeMouseOver: () => highlightDomElement(ancestorGrip),
            onDOMNodeMouseOut: () => unHighlightDomElement(),
          })
        : null;
      return div(
        null,
        div(
          {
            className: "message",
          },
          why.message
        ),
        div(
          {
            className: "mutationNode",
          },
          ancestorRep,
          ancestorGrip
            ? span(
                {
                  className: "why-paused-ancestor",
                },
                React.createElement(Localized, {
                  id:
                    action === "remove"
                      ? "whypaused-mutation-breakpoint-removed"
                      : "whypaused-mutation-breakpoint-added",
                }),
                targetRep
              )
            : targetRep
        )
      );
    }

    if (typeof message == "string") {
      return div(
        {
          className: "message",
        },
        message
      );
    }

    return null;
  }

  render() {
    const { endPanelCollapsed, why } = this.props;
    const { fluentBundles } = this.context;
    const reason = getPauseReason(why);

    if (!why || !reason || endPanelCollapsed) {
      return div({
        className: this.state.hideWhyPaused,
      });
    }
    return (
      // We're rendering the LocalizationProvider component from here and not in an upper
      // component because it does set a new context, overriding the context that we set
      // in the first place in <App>, which breaks some components.
      // This should be fixed in Bug 1743155.
      React.createElement(
        LocalizationProvider,
        {
          bundles: fluentBundles || [],
        },
        div(
          {
            className: "pane why-paused",
          },
          div(
            null,
            div(
              {
                className: "info icon",
              },
              React.createElement(AccessibleImage, {
                className: "info",
              })
            ),
            div(
              {
                className: "pause reason",
              },
              React.createElement(Localized, {
                id: reason,
              }),
              this.renderMessage(why)
            )
          )
        )
      )
    );
  }
}

WhyPaused.contextTypes = { fluentBundles: PropTypes.array };

const mapStateToProps = state => ({
  endPanelCollapsed: getPaneCollapse(state, "end"),
  why: getWhy(state, getCurrentThread(state)),
});

export default connect(mapStateToProps, {
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
})(WhyPaused);
