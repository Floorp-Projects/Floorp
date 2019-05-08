/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry, gToolbox, EVENTS */

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const TreeRow = require("devtools/client/shared/components/tree/TreeRow");
const AuditFilter = createFactory(require("./AuditFilter"));
const AuditController = createFactory(require("./AuditController"));

// Utils
const {flashElementOn, flashElementOff} =
  require("devtools/client/inspector/markup/utils");
const { openDocLink } = require("devtools/client/shared/link");
const { VALUE_FLASHING_DURATION, VALUE_HIGHLIGHT_DURATION } = require("../constants");

// Actions
const { updateDetails } = require("../actions/details");
const { unhighlight } = require("../actions/accessibles");

const { L10N } = require("../utils/l10n");

loader.lazyRequireGetter(this, "Menu", "devtools/client/framework/menu");
loader.lazyRequireGetter(this, "MenuItem", "devtools/client/framework/menu-item");

const { scrollIntoView } = require("devtools/client/shared/scroll");

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

    if (nextProps.highlighted !== this.props.highlighted ||
        nextProps.filtered !== this.props.filtered) {
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
      hasContextMenu: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      walker: PropTypes.object,
    };
  }

  componentDidMount() {
    const { selected, object } = this.props.member;
    if (selected) {
      this.unhighlight();
      this.update();
      this.highlight(object, { duration: VALUE_HIGHLIGHT_DURATION });
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
    const { selected, object } = this.props.member;
    // If row is selected, update corresponding accessible details.
    if (!prevProps.member.selected && selected) {
      this.unhighlight();
      this.update();
      this.highlight(object, { duration: VALUE_HIGHLIGHT_DURATION });
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
    const { dispatch, member: { object }, supports } = this.props;
    if (!gToolbox || !object.actorID) {
      return;
    }

    dispatch(updateDetails(gToolbox.walker, object, supports));
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

  highlight(accessible, options) {
    const { walker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!accessible || !walker) {
      return;
    }

    walker.highlightAccessible(accessible, options).catch(error =>
      console.warn(error));
  }

  unhighlight() {
    const { walker, dispatch } = this.props;
    dispatch(unhighlight());

    if (!walker) {
      return;
    }

    walker.unhighlight().catch(error => console.warn(error));
  }

  async printToJSON() {
    const { member, supports } = this.props;
    if (!supports.snapshot) {
      // Debugger server does not support Accessible actor snapshots.
      return;
    }

    if (gTelemetry) {
      gTelemetry.keyedScalarAdd(TELEMETRY_ACCESSIBLE_CONTEXT_MENU_ITEM_ACTIVATED,
                                "print-to-json", 1);
    }

    const snapshot = await member.object.snapshot();
    openDocLink(`${JSON_URL_PREFIX}${encodeURIComponent(JSON.stringify(snapshot))}`);
  }

  onContextMenu(e) {
    e.stopPropagation();
    e.preventDefault();

    if (!gToolbox) {
      return;
    }

    const menu = new Menu({ id: "accessibility-row-contextmenu" });
    const { supports } = this.props;

    if (supports.snapshot) {
      menu.append(new MenuItem({
        id: "menu-printtojson",
        label: L10N.getStr("accessibility.tree.menu.printToJSON"),
        click: () => this.printToJSON(),
      }));
    }

    menu.popup(e.screenX, e.screenY, gToolbox.doc);

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
      onContextMenu: this.props.hasContextMenu && (e => this.onContextMenu(e)),
      onMouseOver: () => this.highlight(member.object),
      onMouseOut: () => this.unhighlight(),
      key: `${member.path}-${member.active ? "active" : "inactive"}`,
    };

    return AuditController({
      accessible: member.object,
    },
      AuditFilter({},
        HighlightableTreeRow(props)));
  }
}

const mapStateToProps = ({ ui }) => ({
  supports: ui.supports,
});

module.exports =
  connect(mapStateToProps, null, null, { withRef: true })(AccessibilityRow);
