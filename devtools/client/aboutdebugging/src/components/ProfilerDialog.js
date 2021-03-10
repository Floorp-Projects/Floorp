/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("devtools/client/aboutdebugging/src/actions/index");
const Types = require("devtools/client/aboutdebugging/src/types/index");
const {
  PROFILER_PAGE_CONTEXT,
} = require("devtools/client/aboutdebugging/src/constants");

/**
 * This component is a modal dialog containing the performance profiler UI. It uses
 * the simplified DevTools panel located in devtools/client/performance-new. When
 * using a custom preset, and editing the settings, the page context switches
 * to about:profiling, which receives the PerfFront of the remote debuggee.
 */
class ProfilerDialog extends PureComponent {
  static get propTypes() {
    return {
      runtimeDetails: Types.runtimeDetails.isRequired,
      profilerContext: PropTypes.string.isRequired,
      hideProfilerDialog: PropTypes.func.isRequired,
      switchProfilerContext: PropTypes.func.isRequired,
    };
  }

  hide() {
    this.props.hideProfilerDialog();
  }

  setProfilerIframeDirection(frameWindow) {
    // Set iframe direction according to the parent document direction.
    const { documentElement } = document;
    const dir = window.getComputedStyle(documentElement).direction;
    frameWindow.document.documentElement.setAttribute("dir", dir);
  }

  /**
   * The profiler iframe can either be the simplified devtools recording panel,
   * or the more detailed about:profiling settings page.
   */
  renderProfilerIframe() {
    const {
      runtimeDetails: { clientWrapper },
      switchProfilerContext,
      profilerContext,
    } = this.props;

    let src, onLoad;

    switch (profilerContext) {
      case PROFILER_PAGE_CONTEXT.DEVTOOLS_REMOTE:
        src = clientWrapper.getPerformancePanelUrl();
        onLoad = e => {
          const frameWindow = e.target.contentWindow;
          this.setProfilerIframeDirection(frameWindow);

          clientWrapper.loadPerformanceProfiler(frameWindow, () => {
            switchProfilerContext(PROFILER_PAGE_CONTEXT.ABOUTPROFILING_REMOTE);
          });
        };
        break;

      case PROFILER_PAGE_CONTEXT.ABOUTPROFILING_REMOTE:
        src = "about:profiling#remote";
        onLoad = e => {
          const frameWindow = e.target.contentWindow;
          this.setProfilerIframeDirection(frameWindow);

          clientWrapper.loadAboutProfiling(frameWindow, () => {
            switchProfilerContext(PROFILER_PAGE_CONTEXT.DEVTOOLS_REMOTE);
          });
        };
        break;

      default:
        throw new Error(`Unhandled profiler context: "${profilerContext}"`);
    }

    return dom.iframe({
      key: profilerContext,
      className: "profiler-dialog__frame",
      src,
      onLoad,
    });
  }

  render() {
    const { profilerContext, switchProfilerContext } = this.props;
    const dialogSizeClassName =
      profilerContext === PROFILER_PAGE_CONTEXT.DEVTOOLS_REMOTE
        ? "profiler-dialog__inner--medium"
        : "profiler-dialog__inner--large";

    return dom.div(
      {
        className: "profiler-dialog__mask qa-profiler-dialog-mask",
        onClick: () => this.hide(),
      },
      dom.article(
        {
          className: `profiler-dialog__inner ${dialogSizeClassName} qa-profiler-dialog`,
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
              "about-debugging-profiler-dialog-title2"
            )
          ),
          dom.button(
            {
              className: "ghost-button qa-profiler-dialog-close",
              onClick: () => {
                if (profilerContext === PROFILER_PAGE_CONTEXT.DEVTOOLS_REMOTE) {
                  this.hide();
                } else {
                  switchProfilerContext(PROFILER_PAGE_CONTEXT.DEVTOOLS_REMOTE);
                }
              },
            },
            dom.img({
              src: "chrome://devtools/skin/images/close.svg",
            })
          )
        ),
        this.renderProfilerIframe()
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    profilerContext: state.ui.profilerContext,
  };
};

const mapDispatchToProps = {
  hideProfilerDialog: Actions.hideProfilerDialog,
  switchProfilerContext: Actions.switchProfilerContext,
};

module.exports = connect(mapStateToProps, mapDispatchToProps)(ProfilerDialog);
