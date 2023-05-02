/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

const NORMAL_TOOLTIP        = 'tab_closebox_tab_tooltip';
const MULTISELECTED_TOOLTIP = 'tab_closebox_tab_tooltip_multiselected';
const TREE_TOOLTIP          = 'tab_closebox_tree_tooltip';

export const kTAB_CLOSE_BOX_ELEMENT_NAME = 'tab-closebox';

const kTAB_CLOSE_BOX_CLASS_NAME = 'closebox';

export class TabCloseBoxElement extends HTMLElement {
  static define() {
    window.customElements.define(kTAB_CLOSE_BOX_ELEMENT_NAME, TabCloseBoxElement);
  }

  constructor() {
    super();

    // We should initialize private properties with blank value for better performance with a fixed shape.
    this._reservedUpdate = null;

    this.initialized = false;
  }

  connectedCallback() {
    if (this.initialized) {
      this.invalidate();
      return;
    }

    // I make ensure to call these operation only once conservatively because:
    //  * If we do these operations in a constructor of this class, Gecko throws `NotSupportedError: Operation is not supported`.
    //    * I'm not familiar with details of the spec, but this is not Gecko's bug.
    //      See https://dom.spec.whatwg.org/#concept-create-element
    //      "6. If result has children, then throw a "NotSupportedError" DOMException."
    //  * `connectedCallback()` may be called multiple times by append/remove operations.
    //  * `browser.i18n.getMessage()` might be a costly operation.

    // We preserve this class for backward compatibility with other addons.
    this.classList.add(kTAB_CLOSE_BOX_CLASS_NAME);

    this.setAttribute('role', 'button');
    //this.setAttribute('tabindex', '0');

    this.invalidate();
    this.setAttribute('draggable', true); // this is required to cancel click by dragging

    this.initialized = true;
  }

  invalidate() {
    if (this._reservedUpdate)
      return;

    this._reservedUpdate = () => {
      this._reservedUpdate = null;
      this._updateTooltip();
    };
    this.addEventListener('mouseover', this._reservedUpdate, { once: true });
  }

  _updateTooltip() {
    const tab = this.owner;
    if (!tab || !tab.$TST)
      return;

    let key;
    if (tab.$TST.multiselected)
      key = MULTISELECTED_TOOLTIP;
    else if (tab.$TST.hasChild && tab.$TST.subtreeCollapsed)
      key = TREE_TOOLTIP;
    else
      key = NORMAL_TOOLTIP;

    const tooltip = browser.i18n.getMessage(key);
    this.setAttribute('title', tooltip);
  }

  makeAccessible() {
    this.setAttribute('aria-label', browser.i18n.getMessage('tab_closebox_aria_label', [this.owner.id]));
  }
}
