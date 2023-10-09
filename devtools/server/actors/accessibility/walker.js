/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  accessibleWalkerSpec,
} = require("resource://devtools/shared/specs/accessibility.js");

const {
  simulation: { COLOR_TRANSFORMATION_MATRICES },
} = require("resource://devtools/server/actors/accessibility/constants.js");

loader.lazyRequireGetter(
  this,
  "AccessibleActor",
  "resource://devtools/server/actors/accessibility/accessible.js",
  true
);
loader.lazyRequireGetter(
  this,
  ["CustomHighlighterActor"],
  "resource://devtools/server/actors/highlighters.js",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "resource://devtools/shared/DevToolsUtils.js"
);
loader.lazyRequireGetter(
  this,
  "events",
  "resource://devtools/shared/event-emitter.js"
);
loader.lazyRequireGetter(
  this,
  ["getCurrentZoom", "isWindowIncluded", "isFrameWithChildTarget"],
  "resource://devtools/shared/layout/utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "isXUL",
  "resource://devtools/server/actors/highlighters/utils/markup.js",
  true
);
loader.lazyRequireGetter(
  this,
  [
    "isDefunct",
    "loadSheetForBackgroundCalculation",
    "removeSheetForBackgroundCalculation",
  ],
  "resource://devtools/server/actors/utils/accessibility.js",
  true
);
loader.lazyRequireGetter(
  this,
  "accessibility",
  "resource://devtools/shared/constants.js",
  true
);

const kStateHover = 0x00000004; // ElementState::HOVER

const {
  EVENT_TEXT_CHANGED,
  EVENT_TEXT_INSERTED,
  EVENT_TEXT_REMOVED,
  EVENT_ACCELERATOR_CHANGE,
  EVENT_ACTION_CHANGE,
  EVENT_DEFACTION_CHANGE,
  EVENT_DESCRIPTION_CHANGE,
  EVENT_DOCUMENT_ATTRIBUTES_CHANGED,
  EVENT_HIDE,
  EVENT_NAME_CHANGE,
  EVENT_OBJECT_ATTRIBUTE_CHANGED,
  EVENT_REORDER,
  EVENT_STATE_CHANGE,
  EVENT_TEXT_ATTRIBUTE_CHANGED,
  EVENT_VALUE_CHANGE,
} = Ci.nsIAccessibleEvent;

// TODO: We do not need this once bug 1422913 is fixed. We also would not need
// to fire a name change event for an accessible that has an updated subtree and
// that has its name calculated from the said subtree.
const NAME_FROM_SUBTREE_RULE_ROLES = new Set([
  Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
  Ci.nsIAccessibleRole.ROLE_BUTTONMENU,
  Ci.nsIAccessibleRole.ROLE_CELL,
  Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
  Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
  Ci.nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
  Ci.nsIAccessibleRole.ROLE_COLUMNHEADER,
  Ci.nsIAccessibleRole.ROLE_COMBOBOX_OPTION,
  Ci.nsIAccessibleRole.ROLE_DEFINITION,
  Ci.nsIAccessibleRole.ROLE_GRID_CELL,
  Ci.nsIAccessibleRole.ROLE_HEADING,
  Ci.nsIAccessibleRole.ROLE_KEY,
  Ci.nsIAccessibleRole.ROLE_LABEL,
  Ci.nsIAccessibleRole.ROLE_LINK,
  Ci.nsIAccessibleRole.ROLE_LISTITEM,
  Ci.nsIAccessibleRole.ROLE_MATHML_IDENTIFIER,
  Ci.nsIAccessibleRole.ROLE_MATHML_NUMBER,
  Ci.nsIAccessibleRole.ROLE_MATHML_OPERATOR,
  Ci.nsIAccessibleRole.ROLE_MATHML_TEXT,
  Ci.nsIAccessibleRole.ROLE_MATHML_STRING_LITERAL,
  Ci.nsIAccessibleRole.ROLE_MATHML_GLYPH,
  Ci.nsIAccessibleRole.ROLE_MENUITEM,
  Ci.nsIAccessibleRole.ROLE_OPTION,
  Ci.nsIAccessibleRole.ROLE_OUTLINEITEM,
  Ci.nsIAccessibleRole.ROLE_PAGETAB,
  Ci.nsIAccessibleRole.ROLE_PARENT_MENUITEM,
  Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
  Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
  Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
  Ci.nsIAccessibleRole.ROLE_RICH_OPTION,
  Ci.nsIAccessibleRole.ROLE_ROW,
  Ci.nsIAccessibleRole.ROLE_ROWHEADER,
  Ci.nsIAccessibleRole.ROLE_SUMMARY,
  Ci.nsIAccessibleRole.ROLE_SWITCH,
  Ci.nsIAccessibleRole.ROLE_TERM,
  Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
  Ci.nsIAccessibleRole.ROLE_TOOLTIP,
]);

const IS_OSX = Services.appinfo.OS === "Darwin";

const {
  SCORES: { BEST_PRACTICES, FAIL, WARNING },
} = accessibility;

/**
 * Helper function that determines if nsIAccessible object is in stale state. When an
 * object is stale it means its subtree is not up to date.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is stale, false otherwise.
 */
function isStale(accessible) {
  const extraState = {};
  accessible.getState({}, extraState);
  // extraState.value is a bitmask. We are applying bitwise AND to mask out
  // irrelevant states.
  return !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_STALE);
}

/**
 * Get accessibility audit starting with the passed accessible object as a root.
 *
 * @param {Object} acc
 *        AccessibileActor to be used as the root for the audit.
 * @param {Object} options
 *        Options for running audit, may include:
 *        - types: Array of audit types to be performed during audit.
 * @param {Map} report
 *        An accumulator map to be used to store audit information.
 * @param {Object} progress
 *        An audit project object that is used to track the progress of the
 *        audit and send progress "audit-event" events to the client.
 */
function getAudit(acc, options, report, progress) {
  if (acc.isDefunct) {
    return;
  }

  // Audit returns a promise, save the actual value in the report.
  report.set(
    acc,
    acc.audit(options).then(result => {
      report.set(acc, result);
      progress.increment();
    })
  );

  for (const child of acc.children()) {
    getAudit(child, options, report, progress);
  }
}

/**
 * A helper class that is used to track audit progress and send progress events
 * to the client.
 */
class AuditProgress {
  constructor(walker) {
    this.completed = 0;
    this.percentage = 0;
    this.walker = walker;
  }

  setTotal(size) {
    this.size = size;
  }

  notify() {
    this.walker.emit("audit-event", {
      type: "progress",
      progress: {
        total: this.size,
        percentage: this.percentage,
        completed: this.completed,
      },
    });
  }

  increment() {
    this.completed++;
    const { completed, size } = this;
    if (!size) {
      return;
    }

    const percentage = Math.round((completed / size) * 100);
    if (percentage > this.percentage) {
      this.percentage = percentage;
      this.notify();
    }
  }

  destroy() {
    this.walker = null;
  }
}

/**
 * The AccessibleWalkerActor stores a cache of AccessibleActors that represent
 * accessible objects in a given document.
 *
 * It is also responsible for implicitely initializing and shutting down
 * accessibility engine by storing a reference to the XPCOM accessibility
 * service.
 */
class AccessibleWalkerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, accessibleWalkerSpec);
    this.targetActor = targetActor;
    this.refMap = new Map();
    this._loadedSheets = new WeakMap();
    this.setA11yServiceGetter();
    this.onPick = this.onPick.bind(this);
    this.onHovered = this.onHovered.bind(this);
    this._preventContentEvent = this._preventContentEvent.bind(this);
    this.onKey = this.onKey.bind(this);
    this.onFocusIn = this.onFocusIn.bind(this);
    this.onFocusOut = this.onFocusOut.bind(this);
    this.onHighlighterEvent = this.onHighlighterEvent.bind(this);
  }

  get highlighter() {
    if (!this._highlighter) {
      this._highlighter = new CustomHighlighterActor(
        this,
        "AccessibleHighlighter"
      );

      this.manage(this._highlighter);
      this._highlighter.on("highlighter-event", this.onHighlighterEvent);
    }

    return this._highlighter;
  }

  get tabbingOrderHighlighter() {
    if (!this._tabbingOrderHighlighter) {
      this._tabbingOrderHighlighter = new CustomHighlighterActor(
        this,
        "TabbingOrderHighlighter"
      );

      this.manage(this._tabbingOrderHighlighter);
    }

    return this._tabbingOrderHighlighter;
  }

  setA11yServiceGetter() {
    DevToolsUtils.defineLazyGetter(this, "a11yService", () => {
      Services.obs.addObserver(this, "accessible-event");
      return Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
    });
  }

  get rootWin() {
    return this.targetActor && this.targetActor.window;
  }

  get rootDoc() {
    return this.targetActor && this.targetActor.window.document;
  }

  get isXUL() {
    return isXUL(this.rootWin);
  }

  get colorMatrix() {
    if (!this.targetActor.docShell) {
      return null;
    }

    const colorMatrix = this.targetActor.docShell.getColorMatrix();
    if (
      colorMatrix.length === 0 ||
      colorMatrix === COLOR_TRANSFORMATION_MATRICES.NONE
    ) {
      return null;
    }

    return colorMatrix;
  }

  reset() {
    try {
      Services.obs.removeObserver(this, "accessible-event");
    } catch (e) {
      // Accessible event observer might not have been initialized if a11y
      // service was never used.
    }

    this.cancelPick();

    // Clean up accessible actors cache.
    this.clearRefs();

    this._childrenPromise = null;
    delete this.a11yService;
    this.setA11yServiceGetter();
  }

  /**
   * Remove existing cache (of accessible actors) from tree.
   */
  clearRefs() {
    for (const actor of this.refMap.values()) {
      actor.destroy();
    }
  }

  destroy() {
    super.destroy();

    this.reset();

    if (this._highlighter) {
      this._highlighter.off("highlighter-event", this.onHighlighterEvent);
      this._highlighter = null;
    }

    if (this._tabbingOrderHighlighter) {
      this._tabbingOrderHighlighter = null;
    }

    this.targetActor = null;
    this.refMap = null;
  }

  getRef(rawAccessible) {
    return this.refMap.get(rawAccessible);
  }

  addRef(rawAccessible) {
    let actor = this.refMap.get(rawAccessible);
    if (actor) {
      return actor;
    }

    actor = new AccessibleActor(this, rawAccessible);
    // Add the accessible actor as a child of this accessible walker actor,
    // assigning it an actorID.
    this.manage(actor);
    this.refMap.set(rawAccessible, actor);

    return actor;
  }

  /**
   * Clean up accessible actors cache for a given accessible's subtree.
   *
   * @param  {null|nsIAccessible} rawAccessible
   */
  purgeSubtree(rawAccessible) {
    if (!rawAccessible) {
      return;
    }

    try {
      for (
        let child = rawAccessible.firstChild;
        child;
        child = child.nextSibling
      ) {
        this.purgeSubtree(child);
      }
    } catch (e) {
      // rawAccessible or its descendants are defunct.
    }

    const actor = this.getRef(rawAccessible);
    if (actor) {
      actor.destroy();
    }
  }

  unmanage(actor) {
    if (actor instanceof AccessibleActor) {
      this.refMap.delete(actor.rawAccessible);
    }
    Actor.prototype.unmanage.call(this, actor);
  }

  /**
   * A helper method. Accessibility walker is assumed to have only 1 child which
   * is the top level document.
   */
  async children() {
    if (this._childrenPromise) {
      return this._childrenPromise;
    }

    this._childrenPromise = Promise.all([this.getDocument()]);
    const children = await this._childrenPromise;
    this._childrenPromise = null;
    return children;
  }

  /**
   * A promise for a root document accessible actor that only resolves when its
   * corresponding document accessible object is fully loaded.
   *
   * @return {Promise}
   */
  getDocument() {
    if (!this.rootDoc || !this.rootDoc.documentElement) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    if (this.isXUL) {
      const doc = this.addRef(this.getRawAccessibleFor(this.rootDoc));
      return Promise.resolve(doc);
    }

    const doc = this.getRawAccessibleFor(this.rootDoc);

    // For non-visible same-process iframes we don't get a document and
    // won't get a "document-ready" event.
    if (!doc && !this.rootWin.windowGlobalChild.isProcessRoot) {
      // We can ignore such document as there won't be anything to audit in them.
      return null;
    }

    if (!doc || isStale(doc)) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    return Promise.resolve(this.addRef(doc));
  }

  /**
   * Get an accessible actor for a domnode actor.
   * @param  {Object} domNode
   *         domnode actor for which accessible actor is being created.
   * @return {Promse}
   *         A promise that resolves when accessible actor is created for a
   *         domnode actor.
   */
  getAccessibleFor(domNode) {
    // We need to make sure that the document is loaded processed by a11y first.
    return this.getDocument().then(() => {
      const rawAccessible = this.getRawAccessibleFor(domNode.rawNode);
      // Not all DOM nodes have corresponding accessible objects. It's usually
      // the case where there is no semantics or relevance to the accessibility
      // client.
      if (!rawAccessible) {
        return null;
      }

      return this.addRef(rawAccessible);
    });
  }

  /**
   * Get a raw accessible object for a raw node.
   * @param  {DOMNode} rawNode
   *         Raw node for which accessible object is being retrieved.
   * @return {nsIAccessible}
   *         Accessible object for a given DOMNode.
   */
  getRawAccessibleFor(rawNode) {
    // Accessible can only be retrieved iff accessibility service is enabled.
    if (!Services.appinfo.accessibilityEnabled) {
      return null;
    }

    return this.a11yService.getAccessibleFor(rawNode);
  }

  async getAncestry(accessible) {
    if (!accessible || accessible.indexInParent === -1) {
      return [];
    }
    const doc = await this.getDocument();
    if (!doc) {
      return [];
    }

    const ancestry = [];
    if (accessible === doc) {
      return ancestry;
    }

    try {
      let parent = accessible;
      while (parent && (parent = parent.parentAcc) && parent != doc) {
        ancestry.push(parent);
      }
      ancestry.push(doc);
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return ancestry.map(parent => ({
      accessible: parent,
      children: parent.children(),
    }));
  }

  /**
   * Run accessibility audit and return relevant ancestries for AccessibleActors
   * that have non-empty audit checks.
   *
   * @param  {Object} options
   *         Options for running audit, may include:
   *         - types: Array of audit types to be performed during audit.
   *
   * @return {Promise}
   *         A promise that resolves when the audit is complete and all relevant
   *         ancestries are calculated.
   */
  async audit(options) {
    const doc = await this.getDocument();
    if (!doc) {
      return [];
    }

    const report = new Map();
    this._auditProgress = new AuditProgress(this);
    getAudit(doc, options, report, this._auditProgress);
    this._auditProgress.setTotal(report.size);
    await Promise.all(report.values());

    const ancestries = [];
    for (const [acc, audit] of report.entries()) {
      // Filter out audits that have no failing checks.
      if (
        audit &&
        Object.values(audit).some(
          check =>
            check != null &&
            !check.error &&
            [BEST_PRACTICES, FAIL, WARNING].includes(check.score)
        )
      ) {
        ancestries.push(this.getAncestry(acc));
      }
    }

    return Promise.all(ancestries);
  }

  /**
   * Start accessibility audit. The result of this function will not be an audit
   * report. Instead, an "audit-event" event will be fired when the audit is
   * completed or fails.
   *
   * @param {Object} options
   *        Options for running audit, may include:
   *        - types: Array of audit types to be performed during audit.
   */
  startAudit(options) {
    // Audit is already running, wait for the "audit-event" event.
    if (this._auditing) {
      return;
    }

    this._auditing = this.audit(options)
      // We do not want to block on audit request, instead fire "audit-event"
      // event when internal audit is finished or failed.
      .then(ancestries =>
        this.emit("audit-event", {
          type: "completed",
          ancestries,
        })
      )
      .catch(() => this.emit("audit-event", { type: "error" }))
      .finally(() => {
        this._auditing = null;
        if (this._auditProgress) {
          this._auditProgress.destroy();
          this._auditProgress = null;
        }
      });
  }

  onHighlighterEvent(data) {
    this.emit("highlighter-event", data);
  }

  /**
   * Accessible event observer function.
   *
   * @param {Ci.nsIAccessibleEvent} subject
   *                                      accessible event object.
   */
  // eslint-disable-next-line complexity
  observe(subject) {
    const event = subject.QueryInterface(Ci.nsIAccessibleEvent);
    const rawAccessible = event.accessible;
    const accessible = this.getRef(rawAccessible);

    if (rawAccessible instanceof Ci.nsIAccessibleDocument && !accessible) {
      const rootDocAcc = this.getRawAccessibleFor(this.rootDoc);
      if (rawAccessible === rootDocAcc && !isStale(rawAccessible)) {
        this.clearRefs();
        // If it's a top level document notify listeners about the document
        // being ready.
        events.emit(this, "document-ready", rawAccessible);
      }
    }

    switch (event.eventType) {
      case EVENT_STATE_CHANGE:
        const { state, isEnabled } = event.QueryInterface(
          Ci.nsIAccessibleStateChangeEvent
        );
        const isBusy = state & Ci.nsIAccessibleStates.STATE_BUSY;
        if (accessible) {
          // Only propagate state change events for active accessibles.
          if (isBusy && isEnabled) {
            if (rawAccessible instanceof Ci.nsIAccessibleDocument) {
              // Remove existing cache from tree.
              this.clearRefs();
            }
            return;
          }
          events.emit(accessible, "states-change", accessible.states);
        }

        break;
      case EVENT_NAME_CHANGE:
        if (accessible) {
          events.emit(
            accessible,
            "name-change",
            rawAccessible.name,
            event.DOMNode == this.rootDoc
              ? undefined
              : this.getRef(rawAccessible.parent)
          );
        }
        break;
      case EVENT_VALUE_CHANGE:
        if (accessible) {
          events.emit(accessible, "value-change", rawAccessible.value);
        }
        break;
      case EVENT_DESCRIPTION_CHANGE:
        if (accessible) {
          events.emit(
            accessible,
            "description-change",
            rawAccessible.description
          );
        }
        break;
      case EVENT_REORDER:
        if (accessible) {
          accessible
            .children()
            .forEach(child =>
              events.emit(child, "index-in-parent-change", child.indexInParent)
            );
          events.emit(accessible, "reorder", rawAccessible.childCount);
        }
        break;
      case EVENT_HIDE:
        if (event.DOMNode == this.rootDoc) {
          this.clearRefs();
        } else {
          this.purgeSubtree(rawAccessible);
        }
        break;
      case EVENT_DEFACTION_CHANGE:
      case EVENT_ACTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "actions-change", accessible.actions);
        }
        break;
      case EVENT_TEXT_CHANGED:
      case EVENT_TEXT_INSERTED:
      case EVENT_TEXT_REMOVED:
        if (accessible) {
          events.emit(accessible, "text-change");
          if (NAME_FROM_SUBTREE_RULE_ROLES.has(rawAccessible.role)) {
            events.emit(
              accessible,
              "name-change",
              rawAccessible.name,
              event.DOMNode == this.rootDoc
                ? undefined
                : this.getRef(rawAccessible.parent)
            );
          }
        }
        break;
      case EVENT_DOCUMENT_ATTRIBUTES_CHANGED:
      case EVENT_OBJECT_ATTRIBUTE_CHANGED:
      case EVENT_TEXT_ATTRIBUTE_CHANGED:
        if (accessible) {
          events.emit(accessible, "attributes-change", accessible.attributes);
        }
        break;
      // EVENT_ACCELERATOR_CHANGE is currently not fired by gecko accessibility.
      case EVENT_ACCELERATOR_CHANGE:
        if (accessible) {
          events.emit(
            accessible,
            "shortcut-change",
            accessible.keyboardShortcut
          );
        }
        break;
      default:
        break;
    }
  }

  /**
   * Ensure that nothing interferes with the audit for an accessible object
   * (CSS, overlays) by load accessibility highlighter style sheet used for
   * preventing transitions and applying transparency when calculating colour
   * contrast as well as temporarily hiding accessible highlighter overlay.
   * @param  {Object} win
   *         Window where highlighting happens.
   */
  async clearStyles(win) {
    const requests = this._loadedSheets.get(win);
    if (requests != null) {
      this._loadedSheets.set(win, requests + 1);
      return;
    }

    // Disable potential mouse driven transitions (This is important because accessibility
    // highlighter temporarily modifies text color related CSS properties. In case where
    // there are transitions that affect them, there might be unexpected side effects when
    // taking a snapshot for contrast measurement).
    loadSheetForBackgroundCalculation(win);
    this._loadedSheets.set(win, 1);
    await this.hideHighlighter();
  }

  /**
   * Restore CSS and overlays that could've interfered with the audit for an
   * accessible object by unloading accessibility highlighter style sheet used
   * for preventing transitions and applying transparency when calculating
   * colour contrast and potentially restoring accessible highlighter overlay.
   * @param  {Object} win
   *         Window where highlighting was happenning.
   */
  async restoreStyles(win) {
    const requests = this._loadedSheets.get(win);
    if (!requests) {
      return;
    }

    if (requests > 1) {
      this._loadedSheets.set(win, requests - 1);
      return;
    }

    await this.showHighlighter();
    removeSheetForBackgroundCalculation(win);
    this._loadedSheets.delete(win);
  }

  async hideHighlighter() {
    // TODO: Fix this workaround that temporarily removes higlighter bounds
    // overlay that can interfere with the contrast ratio calculation.
    if (this._highlighter) {
      const highlighter = this._highlighter.instance;
      await highlighter.isReady;
      highlighter.hideAccessibleBounds();
    }
  }

  async showHighlighter() {
    // TODO: Fix this workaround that temporarily removes higlighter bounds
    // overlay that can interfere with the contrast ratio calculation.
    if (this._highlighter) {
      const highlighter = this._highlighter.instance;
      await highlighter.isReady;
      highlighter.showAccessibleBounds();
    }
  }

  /**
   * Public method used to show an accessible object highlighter on the client
   * side.
   *
   * @param  {Object} accessible
   *         AccessibleActor to be highlighted.
   * @param  {Object} options
   *         Object used for passing options. Available options:
   *         - duration {Number}
   *                    Duration of time that the highlighter should be shown.
   * @return {Boolean}
   *         True if highlighter shows the accessible object.
   */
  async highlightAccessible(accessible, options = {}) {
    this.unhighlight();
    // Do not highlight if accessible is dead.
    if (!accessible || accessible.isDefunct || accessible.indexInParent < 0) {
      return false;
    }

    this._highlightingAccessible = accessible;
    const { bounds } = accessible;
    if (!bounds) {
      return false;
    }

    const { DOMNode: rawNode } = accessible.rawAccessible;
    const audit = await accessible.audit();
    if (this._highlightingAccessible !== accessible) {
      return false;
    }

    const { name, role } = accessible;
    const { highlighter } = this;
    await highlighter.instance.isReady;
    if (this._highlightingAccessible !== accessible) {
      return false;
    }

    const shown = highlighter.show(
      { rawNode },
      { ...options, ...bounds, name, role, audit, isXUL: this.isXUL }
    );
    this._highlightingAccessible = null;

    return shown;
  }

  /**
   * Public method used to hide an accessible object highlighter on the client
   * side.
   */
  unhighlight() {
    if (!this._highlighter) {
      return;
    }

    this.highlighter.hide();
    this._highlightingAccessible = null;
  }

  /**
   * Picking state that indicates if picking is currently enabled and, if so,
   * what the current and hovered accessible objects are.
   */
  _isPicking = false;
  _currentAccessible = null;

  /**
   * Check is event handling is allowed.
   */
  _isEventAllowed({ view }) {
    return this.rootWin.isChromeWindow || isWindowIncluded(this.rootWin, view);
  }

  /**
   * Check if the DOM event received when picking shold be ignored.
   * @param {Event} event
   */
  _ignoreEventWhenPicking(event) {
    return (
      !this._isPicking ||
      // If the DOM event is about a remote frame, only the WalkerActor for that
      // remote frame target should emit RDP events (hovered/picked/...). And
      // all other WalkerActor for intermediate iframe and top level document
      // targets should stay silent.
      isFrameWithChildTarget(
        this.targetActor,
        event.originalTarget || event.target
      )
    );
  }

  _preventContentEvent(event) {
    if (this._ignoreEventWhenPicking(event)) {
      return;
    }

    event.stopPropagation();
    event.preventDefault();

    const target = event.originalTarget || event.target;
    if (target !== this._currentTarget) {
      this._resetStateAndReleaseTarget();
      this._currentTarget = target;
      // We use InspectorUtils to save the original hover content state of the target
      // element (that includes its hover state). In order to not trigger any visual
      // changes to the element that depend on its hover state we remove the state while
      // the element is the most current target of the highlighter.
      //
      // TODO: This logic can be removed if/when we can use elementsAtPoint API for
      // determining topmost DOMNode that corresponds to specific coordinates. We would
      // then be able to use a highlighter overlay that would prevent all pointer events
      // to content but still render highlighter for the node/element correctly.
      this._currentTargetHoverState =
        InspectorUtils.getContentState(target) & kStateHover;
      InspectorUtils.removeContentState(target, kStateHover);
    }
  }

  /**
   * Click event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current click event.
   */
  onPick(event) {
    if (this._ignoreEventWhenPicking(event)) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    // If shift is pressed, this is only a preview click, send the event to
    // the client, but don't stop picking.
    if (event.shiftKey) {
      if (!this._currentAccessible) {
        this._currentAccessible = this._findAndAttachAccessible(event);
      }
      events.emit(this, "picker-accessible-previewed", this._currentAccessible);
      return;
    }

    this._unsetPickerEnvironment();
    this._isPicking = false;
    if (!this._currentAccessible) {
      this._currentAccessible = this._findAndAttachAccessible(event);
    }
    events.emit(this, "picker-accessible-picked", this._currentAccessible);
  }

  /**
   * Hover event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current hover event.
   */
  async onHovered(event) {
    if (this._ignoreEventWhenPicking(event)) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    const accessible = this._findAndAttachAccessible(event);
    if (!accessible || this._currentAccessible === accessible) {
      return;
    }

    this._currentAccessible = accessible;
    // Highlight current accessible and by the time we are done, if accessible that was
    // highlighted is not current any more (user moved the mouse to a new node) highlight
    // the most current accessible again.
    const shown = await this.highlightAccessible(accessible);
    if (this._isPicking && shown && accessible === this._currentAccessible) {
      events.emit(this, "picker-accessible-hovered", accessible);
    }
  }

  /**
   * Keyboard event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current keyboard event.
   */
  onKey(event) {
    if (!this._currentAccessible || this._ignoreEventWhenPicking(event)) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    /**
     * KEY: Action/scope
     * ENTER/CARRIAGE_RETURN: Picks current accessible
     * ESC/CTRL+SHIFT+C: Cancels picker
     */
    switch (event.keyCode) {
      // Select the element.
      case event.DOM_VK_RETURN:
        this.onPick(event);
        break;
      // Cancel pick mode.
      case event.DOM_VK_ESCAPE:
        this.cancelPick();
        events.emit(this, "picker-accessible-canceled");
        break;
      case event.DOM_VK_C:
        if (
          (IS_OSX && event.metaKey && event.altKey) ||
          (!IS_OSX && event.ctrlKey && event.shiftKey)
        ) {
          this.cancelPick();
          events.emit(this, "picker-accessible-canceled");
        }
        break;
      default:
        break;
    }
  }

  /**
   * Picker method that starts picker content listeners.
   */
  pick() {
    if (!this._isPicking) {
      this._isPicking = true;
      this._setPickerEnvironment();
    }
  }

  /**
   * This pick method also focuses the highlighter's target window.
   */
  pickAndFocus() {
    this.pick();
    this.rootWin.focus();
  }

  attachAccessible(rawAccessible, accessibleDocument) {
    // If raw accessible object is defunct or detached, no need to cache it and
    // its ancestry.
    if (
      !rawAccessible ||
      isDefunct(rawAccessible) ||
      rawAccessible.indexInParent < 0
    ) {
      return null;
    }

    const accessible = this.addRef(rawAccessible);
    // There is a chance that ancestry lookup can fail if the accessible is in
    // the detached subtree. At that point the root accessible object would be
    // defunct and accessing it via parent property will throw.
    try {
      let parent = accessible;
      while (parent && parent.rawAccessible != accessibleDocument) {
        parent = parent.parentAcc;
      }
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return accessible;
  }

  get pixelRatio() {
    return this.rootWin.devicePixelRatio;
  }

  /**
   * Find deepest accessible object that corresponds to the screen coordinates of the
   * mouse pointer and attach it to the AccessibilityWalker tree.
   *
   * @param  {Object} event
   *         Correspoinding content event.
   * @return {null|Object}
   *         Accessible object, if available, that corresponds to a DOM node.
   */
  _findAndAttachAccessible(event) {
    const target = event.originalTarget || event.target;
    const win = target.ownerGlobal;
    // This event might be inside a sub-document, so don't use this.rootDoc.
    const docAcc = this.getRawAccessibleFor(win.document);
    const zoom = this.isXUL ? 1 : getCurrentZoom(win);
    const scale = this.pixelRatio / zoom;
    // If the target is inside a pop-up widget, we need to query the pop-up
    // Accessible, not the DocAccessible. The DocAccessible can't hit test
    // inside pop-ups.
    const popup = win.isChromeWindow ? target.closest("panel") : null;
    const containerAcc = popup ? this.getRawAccessibleFor(popup) : docAcc;
    const rawAccessible = containerAcc.getDeepestChildAtPointInProcess(
      event.screenX * scale,
      event.screenY * scale
    );
    return this.attachAccessible(rawAccessible, docAcc);
  }

  /**
   * Start picker content listeners.
   */
  _setPickerEnvironment() {
    const target = this.targetActor.chromeEventHandler;
    target.addEventListener("mousemove", this.onHovered, true);
    target.addEventListener("click", this.onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("mouseover", this._preventContentEvent, true);
    target.addEventListener("mouseout", this._preventContentEvent, true);
    target.addEventListener("mouseleave", this._preventContentEvent, true);
    target.addEventListener("mouseenter", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this.onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);
  }

  /**
   * If content is still alive, stop picker content listeners, reset the hover state for
   * last target element.
   */
  _unsetPickerEnvironment() {
    const target = this.targetActor.chromeEventHandler;

    if (!target) {
      return;
    }

    target.removeEventListener("mousemove", this.onHovered, true);
    target.removeEventListener("click", this.onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("mouseover", this._preventContentEvent, true);
    target.removeEventListener("mouseout", this._preventContentEvent, true);
    target.removeEventListener("mouseleave", this._preventContentEvent, true);
    target.removeEventListener("mouseenter", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this.onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);

    this._resetStateAndReleaseTarget();
  }

  /**
   * When using accessibility highlighter, we keep track of the most current event pointer
   * event target. In order to update or release the target, we need to make sure we set
   * the content state (using InspectorUtils) to its original value.
   *
   * TODO: This logic can be removed if/when we can use elementsAtPoint API for
   * determining topmost DOMNode that corresponds to specific coordinates. We would then
   * be able to use a highlighter overlay that would prevent all pointer events to content
   * but still render highlighter for the node/element correctly.
   */
  _resetStateAndReleaseTarget() {
    if (!this._currentTarget) {
      return;
    }

    try {
      if (this._currentTargetHoverState) {
        InspectorUtils.setContentState(this._currentTarget, kStateHover);
      }
    } catch (e) {
      // DOMNode is already dead.
    }

    this._currentTarget = null;
    this._currentTargetState = null;
  }

  /**
   * Cacncel picker pick. Remvoe all content listeners and hide the highlighter.
   */
  cancelPick() {
    this.unhighlight();

    if (this._isPicking) {
      this._unsetPickerEnvironment();
      this._isPicking = false;
      this._currentAccessible = null;
    }
  }

  /**
   * Indicates that the tabbing order current active element (focused) is being
   * tracked.
   */
  _isTrackingTabbingOrderFocus = false;

  /**
   * Current focused element in the tabbing order.
   */
  _currentFocusedTabbingOrder = null;

  /**
   * Focusin event handler for when interacting with tabbing order overlay.
   *
   * @param  {Object} event
   *         Most recent focusin event.
   */
  async onFocusIn(event) {
    if (!this._isTrackingTabbingOrderFocus) {
      return;
    }

    const target = event.originalTarget || event.target;
    if (target === this._currentFocusedTabbingOrder) {
      return;
    }

    this._currentFocusedTabbingOrder = target;
    this.tabbingOrderHighlighter._highlighter.updateFocus({
      node: target,
      focused: true,
    });
  }

  /**
   * Focusout event handler for when interacting with tabbing order overlay.
   *
   * @param  {Object} event
   *         Most recent focusout event.
   */
  async onFocusOut(event) {
    if (
      !this._isTrackingTabbingOrderFocus ||
      !this._currentFocusedTabbingOrder
    ) {
      return;
    }

    const target = event.originalTarget || event.target;
    // Sanity check.
    if (target !== this._currentFocusedTabbingOrder) {
      console.warn(
        `focusout target: ${target} does not match current focused element in tabbing order: ${this._currentFocusedTabbingOrder}`
      );
    }

    this.tabbingOrderHighlighter._highlighter.updateFocus({
      node: this._currentFocusedTabbingOrder,
      focused: false,
    });
    this._currentFocusedTabbingOrder = null;
  }

  /**
   * Show tabbing order overlay for a given target.
   *
   * @param  {Object} elm
   *         domnode actor to be used as the starting point for generating the
   *         tabbing order.
   * @param  {Number} index
   *         Starting index for the tabbing order.
   *
   * @return {JSON}
   *         Tabbing order information for the last element in the tabbing
   *         order. It includes a ContentDOMReference for the node and a tabbing
   *         index. If we are at the end of the tabbing order for the top level
   *         content document, the ContentDOMReference will be null. If focus
   *         manager discovered a remote IFRAME, then the ContentDOMReference
   *         references the IFRAME itself.
   */
  showTabbingOrder(elm, index) {
    // Start track focus related events (only once). `showTabbingOrder` will be
    // called multiple times for a given target if it contains other remote
    // targets.
    if (!this._isTrackingTabbingOrderFocus) {
      this._isTrackingTabbingOrderFocus = true;
      const target = this.targetActor.chromeEventHandler;
      target.addEventListener("focusin", this.onFocusIn, true);
      target.addEventListener("focusout", this.onFocusOut, true);
    }

    return this.tabbingOrderHighlighter.show(elm, { index });
  }

  /**
   * Hide tabbing order overlay for a given target.
   */
  hideTabbingOrder() {
    if (!this._tabbingOrderHighlighter) {
      return;
    }

    this.tabbingOrderHighlighter.hide();
    if (!this._isTrackingTabbingOrderFocus) {
      return;
    }

    this._isTrackingTabbingOrderFocus = false;
    this._currentFocusedTabbingOrder = null;
    const target = this.targetActor.chromeEventHandler;
    if (target) {
      target.removeEventListener("focusin", this.onFocusIn, true);
      target.removeEventListener("focusout", this.onFocusOut, true);
    }
  }
}

exports.AccessibleWalkerActor = AccessibleWalkerActor;
