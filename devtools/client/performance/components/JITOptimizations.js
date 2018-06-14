/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/client/locales/jit-optimizations.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const { assert } = require("devtools/shared/DevToolsUtils");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Tree = createFactory(require("devtools/client/shared/components/VirtualizedTree"));
const OptimizationsItem = createFactory(require("./JITOptimizationsItem"));
const FrameView = createFactory(require("../../shared/components/Frame"));
const JIT_TITLE = L10N.getStr("jit.title");
// If TREE_ROW_HEIGHT changes, be sure to change `var(--jit-tree-row-height)`
// in `devtools/client/themes/jit-optimizations.css`
const TREE_ROW_HEIGHT = 14;

/* eslint-disable no-unused-vars */
/**
 * TODO - Re-enable this eslint rule. The JIT tool is a work in progress, and isn't fully
 *        integrated as of yet, and this may represent intended functionality.
 */
const onClickTooltipString = frame =>
  L10N.getFormatStr("viewsourceindebugger",
                    `${frame.source}:${frame.line}:${frame.column}`);
/* eslint-enable no-unused-vars */

const optimizationAttemptModel = {
  id: PropTypes.number.isRequired,
  strategy: PropTypes.string.isRequired,
  outcome: PropTypes.string.isRequired,
};

const optimizationObservedTypeModel = {
  keyedBy: PropTypes.string.isRequired,
  name: PropTypes.string,
  location: PropTypes.string,
  line: PropTypes.string,
};

const optimizationIonTypeModel = {
  id: PropTypes.number.isRequired,
  typeset: PropTypes.arrayOf(optimizationObservedTypeModel),
  site: PropTypes.number.isRequired,
  mirType: PropTypes.number.isRequired,
};

const optimizationSiteModel = {
  id: PropTypes.number.isRequired,
  propertyName: PropTypes.string,
  line: PropTypes.number.isRequired,
  column: PropTypes.number.isRequired,
  data: PropTypes.shape({
    attempts: PropTypes.arrayOf(optimizationAttemptModel).isRequired,
    types: PropTypes.arrayOf(optimizationIonTypeModel).isRequired,
  }).isRequired,
};

class JITOptimizations extends Component {
  static get propTypes() {
    return {
      onViewSourceInDebugger: PropTypes.func.isRequired,
      frameData: PropTypes.object.isRequired,
      optimizationSites: PropTypes.arrayOf(optimizationSiteModel).isRequired,
      autoExpandDepth: PropTypes.number,
    };
  }

  static get defaultProps() {
    return {
      autoExpandDepth: 0
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      expanded: new Set()
    };

    this._createHeader = this._createHeader.bind(this);
    this._createTree = this._createTree.bind(this);
  }

  /**
   * Frame data generated from `frameNode.getInfo()`, or an empty
   * object, as well as a handler for clicking on the frame component.
   *
   * @param {?Object} .frameData
   * @param {Function} .onViewSourceInDebugger
   * @return {ReactElement}
   */
  _createHeader({ frameData, onViewSourceInDebugger }) {
    const { isMetaCategory, url, line } = frameData;
    const name = isMetaCategory ? frameData.categoryData.label :
               frameData.functionName || "";

    // Simulate `SavedFrame`s interface
    const frame = { source: url, line: +line, functionDisplayName: name };

    // Neither Meta Category nodes, or the lack of a selected frame node,
    // renders out a frame source, like "file.js:123"; so just use
    // an empty span.
    let frameComponent;
    if (isMetaCategory || !name) {
      frameComponent = dom.span();
    } else {
      frameComponent = FrameView({
        frame,
        onClick: () => onViewSourceInDebugger(frame),
      });
    }

    return dom.div({ className: "optimization-header" },
      dom.span({ className: "header-title" }, JIT_TITLE),
      dom.span({ className: "header-function-name" }, name),
      frameComponent
    );
  }

  _createTree(props) {
    const {
      autoExpandDepth,
      frameData,
      onViewSourceInDebugger,
      optimizationSites: sites
    } = this.props;

    const getSite = id => sites.find(site => site.id === id);
    const getIonTypeForObserved = type => {
      return getSite(type.id).data.types
        .find(iontype => (iontype.typeset || [])
        .includes(type));
    };
    const isSite = site => getSite(site.id) === site;
    const isAttempts = attempts => getSite(attempts.id).data.attempts === attempts;
    const isAttempt = attempt => getSite(attempt.id).data.attempts.includes(attempt);
    const isTypes = types => getSite(types.id).data.types === types;
    const isType = type => getSite(type.id).data.types.includes(type);
    const isObservedType = type => getIonTypeForObserved(type);

    const getRowType = node => {
      if (isSite(node)) {
        return "site";
      }
      if (isAttempts(node)) {
        return "attempts";
      }
      if (isTypes(node)) {
        return "types";
      }
      if (isAttempt(node)) {
        return "attempt";
      }
      if (isType(node)) {
        return "type";
      }
      if (isObservedType(node)) {
        return "observedtype";
      }
      return null;
    };

    // Creates a unique key for each node in the
    // optimizations data
    const getKey = node => {
      const site = getSite(node.id);
      if (isSite(node)) {
        return node.id;
      } else if (isAttempts(node)) {
        return `${node.id}-A`;
      } else if (isTypes(node)) {
        return `${node.id}-T`;
      } else if (isType(node)) {
        return `${node.id}-T-${site.data.types.indexOf(node)}`;
      } else if (isAttempt(node)) {
        return `${node.id}-A-${site.data.attempts.indexOf(node)}`;
      } else if (isObservedType(node)) {
        const iontype = getIonTypeForObserved(node);
        return `${getKey(iontype)}-O-${iontype.typeset.indexOf(node)}`;
      }
      return "";
    };

    return Tree({
      autoExpandDepth,
      preventNavigationOnArrowRight: false,
      getParent: node => {
        const site = getSite(node.id);
        let parent;
        if (isAttempts(node) || isTypes(node)) {
          parent = site;
        } else if (isType(node)) {
          parent = site.data.types;
        } else if (isAttempt(node)) {
          parent = site.data.attempts;
        } else if (isObservedType(node)) {
          parent = getIonTypeForObserved(node);
        }
        assert(parent, "Could not find a parent for optimization data node");

        return parent;
      },
      getChildren: node => {
        if (isSite(node)) {
          return [node.data.types, node.data.attempts];
        } else if (isAttempts(node) || isTypes(node)) {
          return node;
        } else if (isType(node)) {
          return node.typeset || [];
        }
        return [];
      },
      isExpanded: node => this.state.expanded.has(node),
      onExpand: node => this.setState(state => {
        const expanded = new Set(state.expanded);
        expanded.add(node);
        return { expanded };
      }),
      onCollapse: node => this.setState(state => {
        const expanded = new Set(state.expanded);
        expanded.delete(node);
        return { expanded };
      }),
      onFocus: function() {},
      getKey,
      getRoots: () => sites || [],
      itemHeight: TREE_ROW_HEIGHT,
      renderItem: (item, depth, focused, arrow, expanded) =>
        new OptimizationsItem({
          onViewSourceInDebugger,
          item,
          depth,
          focused,
          arrow,
          expanded,
          type: getRowType(item),
          frameData,
        }),
    });
  }

  render() {
    const header = this._createHeader(this.props);
    const tree = this._createTree(this.props);

    return dom.div({}, header, tree);
  }
}

module.exports = JITOptimizations;
