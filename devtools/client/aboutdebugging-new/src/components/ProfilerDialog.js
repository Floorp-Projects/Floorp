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
    return dom.div(
      {
        className: "profiler-dialog__mask",
        onClick: () => this.hide(),
      },
      dom.article(
        {
          className: "profiler-dialog__inner",
          onClick: e => e.stopPropagation(),
        },
        dom.header(
          {
            className: "profiler-dialog__header",
          },
          Localized(
            {
              id: "about-debugging-profiler-dialog-title",
            },
            dom.h1(
              {
                className: "profiler-dialog__header__title",
              },
              "Performance Profiler",
            )
          ),
          dom.button(
            {
              className: "ghost-button",
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
          src: "chrome://devtools/content/performance-new/index.xhtml",
          onLoad: (e) => {
            const { clientWrapper } = this.props.runtimeDetails;
            clientWrapper.loadPerformanceProfiler(e.target.contentWindow);
          },
        })
      )
    );
  }
}

module.exports = ProfilerDialog;
