/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

import * as Constants from '/common/constants.js';

export const kTAB_FAVICON_ELEMENT_NAME = 'tab-favicon';

const KFAVICON_CLASS_NAME = 'favicon';

const kATTR_NAME_SRC = 'src';

export class TabFaviconElement extends HTMLElement {
  static define() {
    window.customElements.define(kTAB_FAVICON_ELEMENT_NAME, TabFaviconElement);
  }

  static get observedAttributes() {
    return [kATTR_NAME_SRC];
  }

  constructor() {
    super();
  }

  connectedCallback() {
    if (this.initialized) {
      this._applySrc();
      return;
    }

    // I make ensure to call these operation only once conservatively because:
    //  * If we do these operations in a constructor of this class, Gecko throws `NotSupportedError: Operation is not supported`.
    //    * I'm not familiar with details of the spec, but this is not Gecko's bug.
    //      See https://dom.spec.whatwg.org/#concept-create-element
    //      "6. If result has children, then throw a "NotSupportedError" DOMException."
    //  * `connectedCallback()` may be called multiple times by append/remove operations.
    //
    // FIXME:
    //  Ideally, these descendants should be in shadow tree. Thus I don't change these element to custom elements.
    //  However, I hesitate to do it at this moment by these reasons.
    //  If we move these to shadow tree,
    //    * We need some rewrite our style.
    //      * This includes that we need to move almost CSS code into this file as a string.
    //    * I'm not sure about that whether we should require [CSS Shadow Parts](https://bugzilla.mozilla.org/show_bug.cgi?id=1559074).
    //      * I suspect we can resolve almost problems by using CSS Custom Properties.

    // We preserve this class for backward compatibility with other addons.
    this.classList.add(KFAVICON_CLASS_NAME);

    const faviconImage = this.appendChild(document.createElement('img'));
    faviconImage.classList.add(Constants.kFAVICON_IMAGE);
    const src = this.getAttribute(kATTR_NAME_SRC);
    faviconImage.setAttribute(kATTR_NAME_SRC, src);

    const defaultIcon = this.appendChild(document.createElement('span'));
    defaultIcon.classList.add(Constants.kFAVICON_BUILTIN);
    defaultIcon.classList.add(Constants.kFAVICON_DEFAULT); // just for backward compatibility, and this should be removed from future versions

    const throbber = this.appendChild(document.createElement('span'));
    throbber.classList.add(Constants.kTHROBBER);

    this._applySrc();
  }

  get initialized() {
    return !!this._imgElement;
  }

  attributeChangedCallback(name, oldValue, newValue, _namespace) {
    if (oldValue === newValue) {
      return;
    }

    switch (name) {
      case kATTR_NAME_SRC:
        this._applySrc();
        break;

      default:
        throw new RangeError(`Handling \`${name}\` attribute has not been defined.`);
    }
  }

  _applySrc() {
    const img = this._imgElement;
    if (!img)
      return;
    const url = this.src;
    if (url)
      img.setAttribute('src', url);
    else
      img.removeAttribute('src');
  }

  get _imgElement() {
    return this.firstElementChild;
  }

  // These setter/getter is required by webextensions-lib-tab-favicon-helper
  get src() {
    return this.getAttribute(kATTR_NAME_SRC);
  }
  set src(value) {
    if (value)
      this.setAttribute(kATTR_NAME_SRC, value);
    else
      this.removeAttribute(kATTR_NAME_SRC);
  }
}
