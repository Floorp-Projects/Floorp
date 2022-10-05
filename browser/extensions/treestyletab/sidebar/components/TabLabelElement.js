/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

import * as Constants from '/common/constants.js';

export const kTAB_LABEL_ELEMENT_NAME = 'tab-label';

const KLABEL_CLASS_NAME   = 'label';
const kCONTENT_CLASS_NAME = `${KLABEL_CLASS_NAME}-content`;

const kATTR_NAME_VALUE = 'value';

//****************************************************************************
// isRTL https://github.com/kavirajk/isRTL
// The MIT License (MIT)
// Copyright (c) 2013 dhilipsiva
const rtlChars = [
  /* arabic ranges*/
  '\u0600-\u06FF',
  '\u0750-\u077F',
  '\uFB50-\uFDFF',
  '\uFE70-\uFEFF',
  /* hebrew range*/
  '\u05D0-\u05FF'
].join('');

const reRTL = new RegExp(`[${rtlChars}]`, 'g');

function isRTL(text) {
  if (/^\s*\u200f[^\u200e]/.test(text)) // title starting with right-to-left-mark
    return true;
  const textCount = text.replace(/[0-9\s\\\/.,\-+="']/g, '').length; // remove multilengual characters from count
  const rtlCount  = (text.match(reRTL) || []).length;
  return rtlCount >= (textCount-rtlCount) && textCount > 0;
};
//****************************************************************************

export class TabLabelElement extends HTMLElement {
  static define() {
    window.customElements.define(kTAB_LABEL_ELEMENT_NAME, TabLabelElement);
  }

  static get observedAttributes() {
    return [kATTR_NAME_VALUE];
  }

  constructor() {
    super();

    // We should initialize private properties with blank value for better performance with a fixed shape.
    this.__onOverflow = null;
    this.__onUnderflow = null;
  }

  connectedCallback() {
    this.setAttribute('role', 'button');

    if (this.initialized) {
      this._startListening();
      this._applyAttributes();
      this.updateTextContent();
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
    this.classList.add(KLABEL_CLASS_NAME);

    const content = this.appendChild(document.createElement('span'));
    content.classList.add(kCONTENT_CLASS_NAME);

    this._startListening();
    this._applyAttributes();
    this.updateTextContent();
  }

  disconnectedCallback() {
    this._overflowChangeListeners.clear();
    this._endListening();
  }

  get initialized() {
    return !!this._content;
  }

  attributeChangedCallback(name, oldValue, newValue, _namespace) {
    if (oldValue === newValue) {
      return;
    }

    switch (name) {
      case kATTR_NAME_VALUE:
        this.updateTextContent();
        break;

      default:
        throw new RangeError(`Handling \`${name}\` attribute has not been defined.`);
    }
  }

  _applyAttributes() {
    // for convenience on customization with custom user styles
    this._content.setAttribute(Constants.kAPI_TAB_ID, this.getAttribute(Constants.kAPI_TAB_ID));
    this._content.setAttribute(Constants.kAPI_WINDOW_ID, this.getAttribute(Constants.kAPI_WINDOW_ID));
  }

  updateTextContent() {
    const content = this._content;
    if (!content)
      return;
    content.textContent = this.getAttribute(kATTR_NAME_VALUE) || '';
    this.classList.toggle('rtl', isRTL(content.textContent));
  }

  updateOverflow() {
    const tab = this.owner;
    const overflow = tab && !tab.pinned && this._content.getBoundingClientRect().width > this.getBoundingClientRect().width;
    this.classList.toggle('overflow', overflow);
  }

  get _content() {
    return this.querySelector(`.${kCONTENT_CLASS_NAME}`);
  }

  get value() {
    return this.getAttribute(kATTR_NAME_VALUE);
  }
  set value(value) {
    this.setAttribute(kATTR_NAME_VALUE, value);
  }

  get overflow() {
    return this.classList.contains('overflow');
  }

  _startListening() {
    if (this.__onOverflow)
      return;
    this.addEventListener('overflow', this.__onOverflow = this._onOverflow.bind(this));
    this.addEventListener('underflow', this.__onUnderflow = this._onUnderflow.bind(this));
  }

  _endListening() {
    if (!this.__onOverflow)
      return;
    this.removeEventListener('overflow', this.__onOverflow);
    this.removeEventListener('underflow', this.__onUnderflow);
    this.__onOverflow = null;
    this.__onUnderflow = null;
  }

  _onOverflow(_event) {
    this.classList.add('overflow');
    for (const listener of this._overflowChangeListeners) {
      listener();
    }
  }

  _onUnderflow(_event) {
    this.classList.remove('overflow');
    for (const listener of this._overflowChangeListeners) {
      listener();
    }
  }

  addOverflowChangeListener(listener) {
    this._overflowChangeListeners.add(listener);
  }

  removeOverflowChangeListener(listener) {
    this._overflowChangeListeners.delete(listener);
  }

  get _overflowChangeListeners() {
    return this.__overflowChangeListeners || (this.__overflowChangeListeners = new Set());
  }
}
