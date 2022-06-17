/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  const {
    Component,
    createFactory,
    createRef,
  } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");
  const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
  const { tr } = dom;

  // Tree
  const TreeCell = createFactory(
    require("devtools/client/shared/components/tree/TreeCell")
  );
  const LabelCell = createFactory(
    require("devtools/client/shared/components/tree/LabelCell")
  );

  const {
    wrapMoveFocus,
    getFocusableElements,
  } = require("devtools/client/shared/focus");

  const UPDATE_ON_PROPS = [
    "name",
    "open",
    "value",
    "loading",
    "level",
    "selected",
    "active",
    "hasChildren",
  ];

  /**
   * This template represents a node in TreeView component. It's rendered
   * using <tr> element (the entire tree is one big <table>).
   */
  class TreeRow extends Component {
    // See TreeView component for more details about the props and
    // the 'member' object.
    static get propTypes() {
      return {
        member: PropTypes.shape({
          object: PropTypes.object,
          name: PropTypes.string,
          type: PropTypes.string.isRequired,
          rowClass: PropTypes.string.isRequired,
          level: PropTypes.number.isRequired,
          hasChildren: PropTypes.bool,
          value: PropTypes.any,
          open: PropTypes.bool.isRequired,
          path: PropTypes.string.isRequired,
          hidden: PropTypes.bool,
          selected: PropTypes.bool,
          active: PropTypes.bool,
          loading: PropTypes.bool,
        }),
        decorator: PropTypes.object,
        renderCell: PropTypes.func,
        renderLabelCell: PropTypes.func,
        columns: PropTypes.array.isRequired,
        id: PropTypes.string.isRequired,
        provider: PropTypes.object.isRequired,
        onClick: PropTypes.func.isRequired,
        onContextMenu: PropTypes.func,
        onMouseOver: PropTypes.func,
        onMouseOut: PropTypes.func,
      };
    }

    constructor(props) {
      super(props);

      this.treeRowRef = createRef();

      this.getRowClass = this.getRowClass.bind(this);
      this._onKeyDown = this._onKeyDown.bind(this);
    }

    componentDidMount() {
      this._setTabbableState();

      // Child components might add/remove new focusable elements, watch for the
      // additions/removals of descendant nodes and update focusable state.
      const win = this.treeRowRef.current.ownerDocument.defaultView;
      const { MutationObserver } = win;
      this.observer = new MutationObserver(() => {
        this._setTabbableState();
      });
      this.observer.observe(this.treeRowRef.current, {
        childList: true,
        subtree: true,
      });
    }

    // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
    UNSAFE_componentWillReceiveProps(nextProps) {
      // I don't like accessing the underlying DOM elements directly,
      // but this optimization makes the filtering so damn fast!
      // The row doesn't have to be re-rendered, all we really need
      // to do is toggling a class name.
      // The important part is that DOM elements don't need to be
      // re-created when they should appear again.
      if (nextProps.member.hidden != this.props.member.hidden) {
        const row = findDOMNode(this);
        row.classList.toggle("hidden");
      }
    }

    /**
     * Optimize row rendering. If props are the same do not render.
     * This makes the rendering a lot faster!
     */
    shouldComponentUpdate(nextProps) {
      for (const prop of UPDATE_ON_PROPS) {
        if (nextProps.member[prop] != this.props.member[prop]) {
          return true;
        }
      }

      return false;
    }

    componentWillUnmount() {
      this.observer.disconnect();
      this.observer = null;
    }

    /**
     * Makes sure that none of the focusable elements inside the row container
     * are tabbable if the row is not active. If the row is active and focus
     * is outside its container, focus on the first focusable element inside.
     */
    _setTabbableState() {
      const elms = getFocusableElements(this.treeRowRef.current);
      if (elms.length === 0) {
        return;
      }

      const { active } = this.props.member;
      if (!active) {
        elms.forEach(elm => elm.setAttribute("tabindex", "-1"));
        return;
      }

      if (!elms.includes(document.activeElement)) {
        elms[0].focus();
      }
    }

    _onKeyDown(e) {
      const { target, key, shiftKey } = e;

      if (key !== "Tab") {
        return;
      }

      const focusMoved = !!wrapMoveFocus(
        getFocusableElements(this.treeRowRef.current),
        target,
        shiftKey
      );
      if (focusMoved) {
        // Focus was moved to the begining/end of the list, so we need to
        // prevent the default focus change that would happen here.
        e.preventDefault();
      }

      e.stopPropagation();
    }

    getRowClass(object) {
      const decorator = this.props.decorator;
      if (!decorator || !decorator.getRowClass) {
        return [];
      }

      // Decorator can return a simple string or array of strings.
      let classNames = decorator.getRowClass(object);
      if (!classNames) {
        return [];
      }

      if (typeof classNames == "string") {
        classNames = [classNames];
      }

      return classNames;
    }

    render() {
      const member = this.props.member;
      const decorator = this.props.decorator;

      const props = {
        id: this.props.id,
        ref: this.treeRowRef,
        role: "treeitem",
        "aria-level": member.level + 1,
        "aria-selected": !!member.selected,
        onClick: this.props.onClick,
        onContextMenu: this.props.onContextMenu,
        onKeyDownCapture: member.active ? this._onKeyDown : undefined,
        onMouseOver: this.props.onMouseOver,
        onMouseOut: this.props.onMouseOut,
      };

      // Compute class name list for the <tr> element.
      const classNames = this.getRowClass(member.object) || [];
      classNames.push("treeRow");
      classNames.push(member.type + "Row");

      if (member.hasChildren) {
        classNames.push("hasChildren");

        // There are 2 situations where hasChildren is true:
        // 1. it is an object with children. Only set aria-expanded in this situation
        // 2. It is a long string (> 50 chars) that can be expanded to fully display it
        if (member.type !== "string") {
          props["aria-expanded"] = member.open;
        }
      }

      if (member.open) {
        classNames.push("opened");
      }

      if (member.loading) {
        classNames.push("loading");
      }

      if (member.selected) {
        classNames.push("selected");
      }

      if (member.hidden) {
        classNames.push("hidden");
      }

      props.className = classNames.join(" ");

      // The label column (with toggle buttons) is usually
      // the first one, but there might be cases (like in
      // the Memory panel) where the toggling is done
      // in the last column.
      const cells = [];

      // Get components for rendering cells.
      let renderCell = this.props.renderCell || RenderCell;
      let renderLabelCell = this.props.renderLabelCell || RenderLabelCell;
      if (decorator?.renderLabelCell) {
        renderLabelCell =
          decorator.renderLabelCell(member.object) || renderLabelCell;
      }

      // Render a cell for every column.
      this.props.columns.forEach(col => {
        const cellProps = Object.assign({}, this.props, {
          key: col.id,
          id: col.id,
          value: this.props.provider.getValue(member.object, col.id),
        });

        if (decorator?.renderCell) {
          renderCell = decorator.renderCell(member.object, col.id);
        }

        const render = col.id == "default" ? renderLabelCell : renderCell;

        // Some cells don't have to be rendered. This happens when some
        // other cells span more columns. Note that the label cells contains
        // toggle buttons and should be usually there unless we are rendering
        // a simple non-expandable table.
        if (render) {
          cells.push(render(cellProps));
        }
      });

      // Render tree row
      return tr(props, cells);
    }
  }

  // Helpers

  const RenderCell = props => {
    return TreeCell(props);
  };

  const RenderLabelCell = props => {
    return LabelCell(props);
  };

  // Exports from this module
  module.exports = TreeRow;
});
