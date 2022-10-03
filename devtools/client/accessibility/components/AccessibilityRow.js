/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry, EVENTS */

// React & Redux
const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  findDOMNode,
} = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const TreeRow = require("resource://devtools/client/shared/components/tree/TreeRow.js");
const AuditFilter = createFactory(
  require("resource://devtools/client/accessibility/components/AuditFilter.js")
);
const AuditController = createFactory(
  require("resource://devtools/client/accessibility/components/AuditController.js")
);

// Utils
const {
  flashElementOn,
  flashElementOff,
} = require("resource://devtools/client/inspector/markup/utils.js");
const { openDocLink } = require("resource://devtools/client/shared/link.js");
const {
  PREFS,
  VALUE_FLASHING_DURATION,
  VALUE_HIGHLIGHT_DURATION,
} = require("resource://devtools/client/accessibility/constants.js");

const nodeConstants = require("resource://devtools/shared/dom-node-constants.js");

// Actions
const {
  updateDetails,
} = require("resource://devtools/client/accessibility/actions/details.js");
const {
  unhighlight,
} = require("resource://devtools/client/accessibility/actions/accessibles.js");

const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

loader.lazyRequireGetter(
  this,
  "Menu",
  "resource://devtools/client/framework/menu.js"
);
loader.lazyRequireGetter(
  this,
  "MenuItem",
  "resource://devtools/client/framework/menu-item.js"
);

const {
  scrollIntoView,
} = require("resource://devtools/client/shared/scroll.js");

const JSON_URL_PREFIX = "data:application/json;charset=UTF-8,";

const TELEMETRY_ACCESSIBLE_CONTEXT_MENU_OPENED =
  "devtools.accessibility.accessible_context_menu_opened";
const TELEMETRY_ACCESSIBLE_CONTEXT_MENU_ITEM_ACTIVATED =
  "devtools.accessibility.accessible_context_menu_item_activated";

class HighlightableTreeRowClass extends TreeRow {
  shouldComponentUpdate(nextProps) {
    const shouldTreeRowUpdate = super.shouldComponentUpdate(nextProps);
    if (shouldTreeRowUpdate) {
      return shouldTreeRowUpdate;
    }

    if (
      nextProps.highlighted !== this.props.highlighted ||
      nextProps.filtered !== this.props.filtered
    ) {
      return true;
    }

    return false;
  }
}

const HighlightableTreeRow = createFactory(HighlightableTreeRowClass);

// Component that expands TreeView's own TreeRow and is responsible for
// rendering an accessible object.
class AccessibilityRow extends Component {
  static get propTypes() {
    return {
      ...TreeRow.propTypes,
      dispatch: PropTypes.func.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
      scrollContentNodeIntoView: PropTypes.bool.isRequired,
      highlightAccessible: PropTypes.func.isRequired,
      unhighlightAccessible: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    const {
      member: { selected, object },
      scrollContentNodeIntoView,
    } = this.props;
    if (selected) {
      this.unhighlight(object);
      this.update();
      this.highlight(
        object,
        { duration: VALUE_HIGHLIGHT_DURATION },
        scrollContentNodeIntoView
      );
    }

    if (this.props.highlighted) {
      this.scrollIntoView();
    }
  }

  /**
   * Update accessible object details that are going to be rendered inside the
   * accessible panel sidebar.
   */
  componentDidUpdate(prevProps) {
    const {
      member: { selected, object },
      scrollContentNodeIntoView,
    } = this.props;
    // If row is selected, update corresponding accessible details.
    if (!prevProps.member.selected && selected) {
      this.unhighlight(object);
      this.update();
      this.highlight(
        object,
        { duration: VALUE_HIGHLIGHT_DURATION },
        scrollContentNodeIntoView
      );
    }

    if (this.props.highlighted) {
      this.scrollIntoView();
    }

    if (!selected && prevProps.member.value !== this.props.member.value) {
      this.flashValue();
    }
  }

  scrollIntoView() {
    const row = findDOMNode(this);
    // Row might not be rendered in the DOM tree if it is filtered out during
    // audit.
    if (!row) {
      return;
    }

    scrollIntoView(row);
  }

  update() {
    const {
      dispatch,
      member: { object },
    } = this.props;
    if (!object.actorID) {
      return;
    }

    dispatch(updateDetails(object));
    window.emit(EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED, object);
  }

  flashValue() {
    const row = findDOMNode(this);
    // Row might not be rendered in the DOM tree if it is filtered out during
    // audit.
    if (!row) {
      return;
    }

    const value = row.querySelector(".objectBox");

    flashElementOn(value);
    if (this._flashMutationTimer) {
      clearTimeout(this._flashMutationTimer);
      this._flashMutationTimer = null;
    }
    this._flashMutationTimer = setTimeout(() => {
      flashElementOff(value);
    }, VALUE_FLASHING_DURATION);
  }

  /**
   * Scroll the node that corresponds to a current accessible object into view.
   * @param   {Object}
   *          Accessible front that is rendered for this node.
   *
   * @returns {Promise}
   *          Promise that resolves when the node is scrolled into view if
   *          possible.
   */
  async scrollNodeIntoViewIfNeeded(accessibleFront) {
    if (accessibleFront.isDestroyed()) {
      return;
    }

    const domWalker = (await accessibleFront.targetFront.getFront("inspector"))
      .walker;
    if (accessibleFront.isDestroyed()) {
      return;
    }

    const node = await domWalker.getNodeFromActor(accessibleFront.actorID, [
      "rawAccessible",
      "DOMNode",
    ]);
    if (!node) {
      return;
    }

    if (node.nodeType == nodeConstants.ELEMENT_NODE) {
      await node.scrollIntoView();
    } else if (node.nodeType != nodeConstants.DOCUMENT_NODE) {
      // scrollIntoView method is only part of the Element interface, in cases
      // where node is a text node (and not a document node) scroll into view
      // its parent.
      await node.parentNode().scrollIntoView();
    }
  }

  async highlight(accessibleFront, options, scrollContentNodeIntoView) {
    this.props.dispatch(unhighlight());
    // If necessary scroll the node into view before showing the accessibility
    // highlighter.
    if (scrollContentNodeIntoView) {
      await this.scrollNodeIntoViewIfNeeded(accessibleFront);
    }

    this.props.highlightAccessible(accessibleFront, options);
  }

  unhighlight(accessibleFront) {
    this.props.dispatch(unhighlight());
    this.props.unhighlightAccessible(accessibleFront);
  }

  async printToJSON() {
    if (gTelemetry) {
      gTelemetry.keyedScalarAdd(
        TELEMETRY_ACCESSIBLE_CONTEXT_MENU_ITEM_ACTIVATED,
        "print-to-json",
        1
      );
    }

    const snapshot = await this.props.member.object.snapshot();
    openDocLink(
      `${JSON_URL_PREFIX}${encodeURIComponent(JSON.stringify(snapshot))}`
    );
  }

  onContextMenu(e) {
    e.stopPropagation();
    e.preventDefault();

    if (!this.props.toolboxDoc) {
      return;
    }

    const menu = new Menu({ id: "accessibility-row-contextmenu" });
    menu.append(
      new MenuItem({
        id: "menu-printtojson",
        label: L10N.getStr("accessibility.tree.menu.printToJSON"),
        click: () => this.printToJSON(),
      })
    );

    menu.popup(e.screenX, e.screenY, this.props.toolboxDoc);

    if (gTelemetry) {
      gTelemetry.scalarAdd(TELEMETRY_ACCESSIBLE_CONTEXT_MENU_OPENED, 1);
    }
  }

  /**
   * Render accessible row component.
   * @returns acecssible-row React component.
   */
  render() {
    const { member } = this.props;
    const props = {
      ...this.props,
      onContextMenu: e => this.onContextMenu(e),
      onMouseOver: () => this.highlight(member.object),
      onMouseOut: () => this.unhighlight(member.object),
      key: `${member.path}-${member.active ? "active" : "inactive"}`,
    };

    return AuditController(
      {
        accessibleFront: member.object,
      },
      AuditFilter({}, HighlightableTreeRow(props))
    );
  }
}

const mapStateToProps = ({
  ui: { [PREFS.SCROLL_INTO_VIEW]: scrollContentNodeIntoView },
}) => ({
  scrollContentNodeIntoView,
});

module.exports = connect(mapStateToProps, null, null, { withRef: true })(
  AccessibilityRow
);
