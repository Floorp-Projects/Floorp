/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

import {
  configs
} from '/common/common.js';
import * as Constants from '/common/constants.js';

export const kTAB_COUNTER_ELEMENT_NAME = 'tab-counter';

const kTAB_COUNTER_CLASS_NAME = 'counter';

export class TabCounterElement extends HTMLElement {
  static define() {
    window.customElements.define(kTAB_COUNTER_ELEMENT_NAME, TabCounterElement);
  }

  constructor() {
    super();

    this.initialized = false;
  }

  connectedCallback() {
    if (this.initialized) {
      this.update();
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
    this.classList.add(kTAB_COUNTER_CLASS_NAME);

    this.update();

    this.initialized = true;
  }

  update() {
    const tab = this.owner;
    if (!tab || !tab.$TST)
      return;

    const descendants = tab.$TST.descendants;
    let count = descendants.length;
    if (configs.counterRole == Constants.kCOUNTER_ROLE_ALL_TABS)
      count += 1;
    this.textContent = count;
  }
}
