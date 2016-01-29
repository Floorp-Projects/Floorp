/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const STRINGS_URI = "chrome://devtools/locale/jit-optimizations.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const { DOM: dom, PropTypes, createClass, createFactory } = require("devtools/client/shared/vendor/react");
const Frame = createFactory(require("devtools/client/shared/components/frame"));
const OPTIMIZATION_FAILURE = L10N.getStr("jit.optimizationFailure");
const JIT_SAMPLES = L10N.getStr("jit.samples");
const JIT_EMPTY_TEXT = L10N.getStr("jit.empty");
const PROPNAME_MAX_LENGTH = 4;
// If TREE_ROW_HEIGHT changes, be sure to change `var(--jit-tree-row-height)`
// in `devtools/client/themes/jit-optimizations.css`
const TREE_ROW_HEIGHT = 14;

const OPTIMIZATION_ITEM_TYPES = ["site", "attempts", "types", "attempt", "type", "observedtype"];
const OptimizationsItem = module.exports = createClass({
  displayName: "OptimizationsItem",

  propTypes: {
    onViewSourceInDebugger: PropTypes.func.isRequired,
    frameData: PropTypes.object.isRequired,
    type: PropTypes.oneOf(OPTIMIZATION_ITEM_TYPES).isRequired,
  },

  render() {
    let {
      item,
      depth,
      arrow,
      focused,
      type,
      frameData,
      onViewSourceInDebugger,
    } = this.props;

    let content;
    switch (type) {
      case "site":         content = this._renderSite(this.props); break;
      case "attempts":     content = this._renderAttempts(this.props); break;
      case "types":        content = this._renderTypes(this.props); break;
      case "attempt":      content = this._renderAttempt(this.props); break;
      case "type":         content = this._renderType(this.props); break;
      case "observedtype": content = this._renderObservedType(this.props); break;
    };

    return dom.div(
      {
        className: `optimization-tree-item optimization-tree-item-${type}`,
        style: { marginLeft: depth * TREE_ROW_HEIGHT }
      },
      arrow,
      content
    );
  },

  _renderSite({ item: site, onViewSourceInDebugger, frameData }) {
    let attempts = site.data.attempts;
    let lastStrategy = attempts[attempts.length - 1].strategy;
    let propString = "";
    let propertyName = site.data.propertyName;

    // Display property name if it exists
    if (propertyName) {
      if (propertyName.length > PROPNAME_MAX_LENGTH) {
        propString = ` (.${propertyName.substr(0, PROPNAME_MAX_LENGTH)}…)`;
      } else {
        propString = ` (.${propertyName})`;
      }
    }

    let sampleString = PluralForm.get(site.samples, JIT_SAMPLES).replace("#1", site.samples);
    let text = `${lastStrategy}${propString} – (${sampleString})`;
    let frame = Frame({
      onClick: () => onViewSourceInDebugger(frameData.url, site.data.line),
      frame: {
        source: frameData.url,
        line: site.data.line,
        column: site.data.column,
      }
    })
    let children = [text, frame];

    if (!site.hasSuccessfulOutcome()) {
      children.unshift(dom.span({ className: "opt-icon warning" }));
    }

    return dom.span({ className: "optimization-site" }, ...children);
  },

  _renderAttempts({ item: attempts }) {
    return dom.span({ className: "optimization-attempts" },
      `Attempts (${attempts.length})`
    );
  },

  _renderTypes({ item: types }) {
    return dom.span({ className: "optimization-types" },
      `Types (${types.length})`
    );
  },

  _renderAttempt({ item: attempt }) {
    let success = JITOptimizations.isSuccessfulOutcome(attempt.outcome);
    let { strategy, outcome } = attempt;
    return dom.span({ className: "optimization-attempt" },
      dom.span({ className: "optimization-strategy" }, strategy),
      " → ",
      dom.span({ className: `optimization-outcome ${success ? "success" : "failure"}` }, outcome)
    );
  },

  _renderType({ item: type }) {
    return dom.span({ className: "optimization-ion-type" }, `${type.site}:${type.mirType}`);
  },

  _renderObservedType({ onViewSourceInDebugger, item: type }) {
    let children = [
      `${type.keyedBy}${type.name ? ` → ${type.name}` : ""}`
    ];

    // If we have a line and location, make a link to the debugger
    if (type.location && type.line) {
      children.push(
        Frame({
          onClick: () => onViewSourceInDebugger(type.location, type.line),
          frame: {
            source: type.location,
            line: type.line,
            column: type.column,
          }
        })
      );
    }
    // Otherwise if we just have a location, it's probably just a memory location.
    else if (type.location) {
      children.push(`@${type.location}`);
    }

    return dom.span({ className: "optimization-observed-type" }, ...children);
  },
});
