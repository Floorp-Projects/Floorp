/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol.js");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec,
  parentAccessibilitySpec,
  simulatorSpec,
} = require("devtools/shared/specs/accessibility");
const events = require("devtools/shared/event-emitter");
const BROWSER_TOOLBOX_FISSION_ENABLED = Services.prefs.getBoolPref(
  "devtools.browsertoolbox.fission",
  false
);

class AccessibleFront extends FrontClassWithSpec(accessibleSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.before("audited", this.audited.bind(this));
    this.before("name-change", this.nameChange.bind(this));
    this.before("value-change", this.valueChange.bind(this));
    this.before("description-change", this.descriptionChange.bind(this));
    this.before("shortcut-change", this.shortcutChange.bind(this));
    this.before("reorder", this.reorder.bind(this));
    this.before("text-change", this.textChange.bind(this));
    this.before("index-in-parent-change", this.indexInParentChange.bind(this));
    this.before("states-change", this.statesChange.bind(this));
    this.before("actions-change", this.actionsChange.bind(this));
    this.before("attributes-change", this.attributesChange.bind(this));
  }

  marshallPool() {
    return this.getParent();
  }

  get useChildTargetToFetchChildren() {
    if (!BROWSER_TOOLBOX_FISSION_ENABLED && this.targetFront.isParentProcess) {
      return false;
    }

    return this._form.useChildTargetToFetchChildren;
  }

  get role() {
    return this._form.role;
  }

  get name() {
    return this._form.name;
  }

  get value() {
    return this._form.value;
  }

  get description() {
    return this._form.description;
  }

  get keyboardShortcut() {
    return this._form.keyboardShortcut;
  }

  get childCount() {
    return this._form.childCount;
  }

  get domNodeType() {
    return this._form.domNodeType;
  }

  get indexInParent() {
    return this._form.indexInParent;
  }

  get states() {
    return this._form.states;
  }

  get actions() {
    return this._form.actions;
  }

  get attributes() {
    return this._form.attributes;
  }

  get checks() {
    return this._form.checks;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = this._form || {};
    Object.assign(this._form, form);
  }

  nameChange(name, parent) {
    this._form.name = name;
    // Name change event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    const accessibilityWalkerFront = this.getParent();
    if (accessibilityWalkerFront) {
      events.emit(accessibilityWalkerFront, "name-change", this, parent);
    }
  }

  valueChange(value) {
    this._form.value = value;
  }

  descriptionChange(description) {
    this._form.description = description;
  }

  shortcutChange(keyboardShortcut) {
    this._form.keyboardShortcut = keyboardShortcut;
  }

  reorder(childCount) {
    this._form.childCount = childCount;
    // Reorder event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    const accessibilityWalkerFront = this.getParent();
    if (accessibilityWalkerFront) {
      events.emit(accessibilityWalkerFront, "reorder", this);
    }
  }

  textChange() {
    // Text event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    const accessibilityWalkerFront = this.getParent();
    if (accessibilityWalkerFront) {
      events.emit(accessibilityWalkerFront, "text-change", this);
    }
  }

  indexInParentChange(indexInParent) {
    this._form.indexInParent = indexInParent;
  }

  statesChange(states) {
    this._form.states = states;
  }

  actionsChange(actions) {
    this._form.actions = actions;
  }

  attributesChange(attributes) {
    this._form.attributes = attributes;
  }

  audited(checks) {
    this._form.checks = this._form.checks || {};
    Object.assign(this._form.checks, checks);
  }

  hydrate() {
    return super.hydrate().then(properties => {
      Object.assign(this._form, properties);
    });
  }

  async children() {
    if (!this.useChildTargetToFetchChildren) {
      return super.children();
    }

    const { walker: domWalkerFront } = await this.targetFront.getFront(
      "inspector"
    );
    const node = await domWalkerFront.getNodeFromActor(this.actorID, [
      "rawAccessible",
      "DOMNode",
    ]);
    // We are using DOM inspector/walker API here because we want to keep both
    // the accessiblity tree and the DOM tree in sync. This is necessary for
    // several features that the accessibility panel provides such as inspecting
    // a corresponding DOM node or any other functionality that requires DOM
    // node ancestries to be resolved all the way up to the top level document.
    const {
      nodes: [documentNodeFront],
    } = await domWalkerFront.children(node);
    const accessibilityFront = await documentNodeFront.targetFront.getFront(
      "accessibility"
    );

    return accessibilityFront.accessibleWalkerFront.children();
  }

  /**
   * Helper function that helps with building a complete snapshot of
   * accessibility tree starting at the level of current accessible front. It
   * accumulates subtrees from possible out of process frames that are children
   * of the current accessible front.
   * @param  {JSON} snapshot
   *         Snapshot of the current accessible front or one of its in process
   *         children when recursing.
   *
   * @return {JSON}
   *         Complete snapshot of current accessible front.
   */
  async _accumulateSnapshot(snapshot) {
    const { childCount, useChildTargetToFetchChildren } = snapshot;
    // No children, we are done.
    if (childCount === 0) {
      return snapshot;
    }

    // If current accessible is not a remote frame, continue accumulating inside
    // its children.
    if (!useChildTargetToFetchChildren) {
      const childSnapshots = [];
      for (const childSnapshot of snapshot.children) {
        childSnapshots.push(this._accumulateSnapshot(childSnapshot));
      }
      await Promise.all(childSnapshots);
      return snapshot;
    }

    // When we have a remote frame, we need to obtain an accessible front for a
    // remote frame document and retrieve its snapshot.
    const inspectorFront = await this.targetFront.getFront("inspector");
    const frameNodeFront = await inspectorFront.getNodeActorFromContentDomReference(
      snapshot.contentDOMReference
    );
    // Remove contentDOMReference and useChildTargetToFetchChildren properties.
    delete snapshot.contentDOMReference;
    delete snapshot.useChildTargetToFetchChildren;
    if (!frameNodeFront) {
      return snapshot;
    }

    // Remote frame lives in the same process as the current accessible
    // front we can retrieve the accessible front directly.
    const frameAccessibleFront = await this.parentFront.getAccessibleFor(
      frameNodeFront
    );
    if (!frameAccessibleFront) {
      return snapshot;
    }

    const [docAccessibleFront] = await frameAccessibleFront.children();
    const childSnapshot = await docAccessibleFront.snapshot();
    snapshot.children.push(childSnapshot);

    return snapshot;
  }

  /**
   * Retrieves a complete JSON snapshot for an accessible subtree of a given
   * accessible front (inclduing OOP frames).
   */
  async snapshot() {
    const snapshot = await super.snapshot();
    await this._accumulateSnapshot(snapshot);
    return snapshot;
  }
}

class AccessibleWalkerFront extends FrontClassWithSpec(accessibleWalkerSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.documentReady = this.documentReady.bind(this);
    this.on("document-ready", this.documentReady);
  }

  destroy() {
    this.off("document-ready", this.documentReady);
    super.destroy();
  }

  form(json) {
    this.actorID = json.actor;
  }

  documentReady() {
    if (this.targetFront.isTopLevel) {
      this.emit("top-level-document-ready");
    }
  }

  pick(doFocus) {
    if (doFocus) {
      return this.pickAndFocus();
    }

    return super.pick();
  }

  /**
   * Get the accessible object ancestry starting from the given accessible to
   * the top level document. BROWSER_TOOLBOX_FISSION_ENABLED is false, the top
   * level document is bound by current target's document. Otherwise, the top
   * level document is in the top level content process.
   * @param  {Object} accessible
   *         Accessible front to determine the ancestry for.
   *
   * @return {Array}  ancestry
   *         List of ancestry objects which consist of an accessible with its
   *         children.
   */
  async getAncestry(accessible) {
    const ancestry = await super.getAncestry(accessible);

    if (!BROWSER_TOOLBOX_FISSION_ENABLED && this.targetFront.isParentProcess) {
      // Do not try to get the ancestry across the remote frame hierarchy.
      return ancestry;
    }

    const parentTarget = await this.targetFront.getParentTarget();
    if (!parentTarget) {
      return ancestry;
    }

    // Get an accessible front for the parent frame. We go through the
    // inspector's walker to keep both inspector and accessibility trees in
    // sync.
    const { walker: domWalkerFront } = await this.targetFront.getFront(
      "inspector"
    );
    const frameNodeFront = (await domWalkerFront.getRootNode()).parentNode();
    const accessibilityFront = await parentTarget.getFront("accessibility");
    const { accessibleWalkerFront } = accessibilityFront;
    const frameAccessibleFront = await accessibleWalkerFront.getAccessibleFor(
      frameNodeFront
    );

    if (!frameAccessibleFront) {
      // Most likely we are inside a hidden frame.
      return Promise.reject(
        `Can't get the ancestry for an accessible front ${accessible.actorID}. It is in the detached tree.`
      );
    }

    // Compose the final ancestry out of ancestry for the given accessible in
    // the current process and recursively get the ancestry for the frame
    // accessible.
    ancestry.push(
      {
        accessible: frameAccessibleFront,
        children: await frameAccessibleFront.children(),
      },
      ...(await accessibleWalkerFront.getAncestry(frameAccessibleFront))
    );

    return ancestry;
  }

  /**
   * Run an accessibility audit for a document that accessibility walker is
   * responsible for (in process). In addition to plainly running an audit (in
   * cases when the document is in the OOP frame), this method also updates
   * relative ancestries of audited accessible objects all the way up to the top
   * level document for the toolbox.
   * @param {Object} options
   *                 - {Array}    types
   *                   types of the accessibility issues to audit for
   *                 - {Function} onProgress
   *                   callback function for a progress audit-event
   *                 - {Boolean} retrieveAncestries (defaults to true)
   *                   Set to false to _not_ retrieve ancestries of audited accessible objects.
   *                   This is used when a specific document is selected in the iframe picker
   *                   and we want to treat it as the root of the accessibility panel tree.
   */
  async audit({ types, onProgress, retrieveAncestries = true }) {
    const onAudit = new Promise(resolve => {
      const auditEventHandler = ({ type, ancestries, progress }) => {
        switch (type) {
          case "error":
            this.off("audit-event", auditEventHandler);
            resolve({ error: true });
            break;
          case "completed":
            this.off("audit-event", auditEventHandler);
            resolve({ ancestries });
            break;
          case "progress":
            onProgress(progress);
            break;
          default:
            break;
        }
      };

      this.on("audit-event", auditEventHandler);
      super.startAudit({ types });
    });

    const audit = await onAudit;
    // If audit resulted in an error, if there's nothing to report or if the callsite
    // explicitly asked to not retrieve ancestries, we are done.
    // (no need to check for ancestry across the remote frame hierarchy).
    // See also https://bugzilla.mozilla.org/show_bug.cgi?id=1641551 why the rest of
    // the code path is only supported when content toolbox fission is enabled.
    if (audit.error || audit.ancestries.length === 0 || !retrieveAncestries) {
      return audit;
    }

    const parentTarget = await this.targetFront.getParentTarget();
    // If there is no parent target, we do not need to update ancestries as we
    // are in the top level document.
    if (!parentTarget) {
      return audit;
    }

    // Retrieve an ancestry (cross process) for a current root document and make
    // audit report ancestries relative to it.
    const [docAccessibleFront] = await this.children();
    let docAccessibleAncestry;
    try {
      docAccessibleAncestry = await this.getAncestry(docAccessibleFront);
    } catch (e) {
      // We are in a detached subtree. We do not consider this an error, instead
      // we need to ignore the audit for this frame and return an empty report.
      return { ancestries: [] };
    }
    for (const ancestry of audit.ancestries) {
      // Compose the final ancestries out of the ones in the audit report
      // relative to this document and the ancestry of the document itself
      // (cross process).
      ancestry.push(...docAccessibleAncestry);
    }

    return audit;
  }

  /**
   * A helper wrapper function to show tabbing order overlay for a given target.
   * The only additional work done is resolving domnode front from a
   * ContentDOMReference received from a remote target.
   *
   * @param  {Object} startElm
   *         domnode front to be used as the starting point for generating the
   *         tabbing order.
   * @param  {Number} startIndex
   *         Starting index for the tabbing order.
   */
  async _showTabbingOrder(startElm, startIndex) {
    const { contentDOMReference, index } = await super.showTabbingOrder(
      startElm,
      startIndex
    );
    let elm;
    if (contentDOMReference) {
      const inspectorFront = await this.targetFront.getFront("inspector");
      elm = await inspectorFront.getNodeActorFromContentDomReference(
        contentDOMReference
      );
    }

    return { elm, index };
  }

  /**
   * Show tabbing order overlay for a given target.
   *
   * @param  {Object} startElm
   *         domnode front to be used as the starting point for generating the
   *         tabbing order.
   * @param  {Number} startIndex
   *         Starting index for the tabbing order.
   *
   * @return {JSON}
   *         Tabbing order information for the last element in the tabbing
   *         order. It includes a domnode front and a tabbing index. If we are
   *         at the end of the tabbing order for the top level content document,
   *         the domnode front will be null. If focus manager discovered a
   *         remote IFRAME, then the domnode front is for the IFRAME itself.
   */
  async showTabbingOrder(startElm, startIndex) {
    let { elm: currentElm, index: currentIndex } = await this._showTabbingOrder(
      startElm,
      startIndex
    );

    // If no remote frames were found, currentElm will be null.
    while (currentElm) {
      // Safety check to ensure that the currentElm is a remote frame.
      if (currentElm.useChildTargetToFetchChildren) {
        const {
          walker: domWalkerFront,
        } = await currentElm.targetFront.getFront("inspector");
        const {
          nodes: [childDocumentNodeFront],
        } = await domWalkerFront.children(currentElm);
        const {
          accessibleWalkerFront,
        } = await childDocumentNodeFront.targetFront.getFront("accessibility");
        // Show tabbing order in the remote target, while updating the tabbing
        // index.
        ({ index: currentIndex } = await accessibleWalkerFront.showTabbingOrder(
          childDocumentNodeFront,
          currentIndex
        ));
      }

      // Finished with the remote frame, continue in tabbing order, from the
      // remote frame.
      ({ elm: currentElm, index: currentIndex } = await this._showTabbingOrder(
        currentElm,
        currentIndex
      ));
    }

    return { elm: currentElm, index: currentIndex };
  }
}

class AccessibilityFront extends FrontClassWithSpec(accessibilitySpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.before("init", this.init.bind(this));
    this.before("shutdown", this.shutdown.bind(this));

    // Attribute name from which to retrieve the actorID out of the target
    // actor's form
    this.formAttributeName = "accessibilityActor";
  }

  async initialize() {
    this.accessibleWalkerFront = await super.getWalker();
    this.simulatorFront = await super.getSimulator();
    const { enabled } = await super.bootstrap();
    this.enabled = enabled;

    try {
      this._traits = await this.getTraits();
    } catch (e) {
      // @backward-compat { version 84 } getTraits isn't available on older server.
      this._traits = {};
    }
  }

  get traits() {
    return this._traits;
  }

  init() {
    this.enabled = true;
  }

  shutdown() {
    this.enabled = false;
  }
}

class ParentAccessibilityFront extends FrontClassWithSpec(
  parentAccessibilitySpec
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.before("can-be-enabled-change", this.canBeEnabled.bind(this));
    this.before("can-be-disabled-change", this.canBeDisabled.bind(this));

    // Attribute name from which to retrieve the actorID out of the target
    // actor's form
    this.formAttributeName = "parentAccessibilityActor";
  }

  async initialize() {
    ({
      canBeEnabled: this.canBeEnabled,
      canBeDisabled: this.canBeDisabled,
    } = await super.bootstrap());
  }

  canBeEnabled(canBeEnabled) {
    this.canBeEnabled = canBeEnabled;
  }

  canBeDisabled(canBeDisabled) {
    this.canBeDisabled = canBeDisabled;
  }
}

const SimulatorFront = FrontClassWithSpec(simulatorSpec);

exports.AccessibleFront = AccessibleFront;
registerFront(AccessibleFront);
exports.AccessibleWalkerFront = AccessibleWalkerFront;
registerFront(AccessibleWalkerFront);
exports.AccessibilityFront = AccessibilityFront;
registerFront(AccessibilityFront);
exports.ParentAccessibilityFront = ParentAccessibilityFront;
registerFront(ParentAccessibilityFront);
exports.SimulatorFront = SimulatorFront;
registerFront(SimulatorFront);
