/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../actions/index");
const Types = require("../types/index");

/**
 * This component a modal dialog containing the performance profiler UI.
 */
class ProfilerDialog extends PureComponent {
  static get propTypes() {
    return {
      runtimeDetails: Types.runtimeDetails.isRequired,
      dispatch: PropTypes.func.isRequired,
    };
  }

  hide() {
    this.props.dispatch(Actions.hideProfilerDialog());
  }

  render() {
    const { clientWrapper } = this.props.runtimeDetails;

    return dom.div(
      {
        className: "profiler-dialog__mask qa-profiler-dialog-mask",
        onClick: () => this.hide(),
      },
      dom.article(
        {
          className: "profiler-dialog__inner qa-profiler-dialog",
          onClick: e => e.stopPropagation(),
        },
        dom.header(
          {
            className: "profiler-dialog__header",
          },
          Localized(
            {
              id: "about-debugging-profiler-dialog-title2",
            },
            dom.h1(
              {
                className: "profiler-dialog__header__title",
              },
              "about-debugging-profiler-dialog-title2",
            )
          ),
          dom.button(
            {
              className: "ghost-button qa-profiler-dialog-close",
              onClick: () => this.hide(),
            },
            dom.img(
              {
                src: "chrome://devtools/skin/images/close.svg",
              }
            )
          )
        ),
        dom.iframe({
          className: "profiler-dialog__frame",
          src: clientWrapper.getPerformancePanelUrl(),
          onLoad: (e) => {
            clientWrapper.loadPerformanceProfiler(e.target.contentWindow);
          },
        })
      )
    );
  }
}

module.exports = ProfilerDialog;
