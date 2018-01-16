/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */

/* Copyright 2017 Mozilla Foundation and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/* fluent@0.4.1 */

const { Localization } =
  Components.utils.import("resource://gre/modules/Localization.jsm", {});

// Match the opening angle bracket (<) in HTML tags, and HTML entities like
// &amp;, &#0038;, &#x0026;.
const reOverlay = /<|&#?\w+;/;

/**
 * The list of elements that are allowed to be inserted into a localization.
 *
 * Source: https://www.w3.org/TR/html5/text-level-semantics.html
 */
const ALLOWED_ELEMENTS = {
  'http://www.w3.org/1999/xhtml': [
    'a', 'em', 'strong', 'small', 's', 'cite', 'q', 'dfn', 'abbr', 'data',
    'time', 'code', 'var', 'samp', 'kbd', 'sub', 'sup', 'i', 'b', 'u',
    'mark', 'ruby', 'rt', 'rp', 'bdi', 'bdo', 'span', 'br', 'wbr'
  ],
};

const ALLOWED_ATTRIBUTES = {
  'http://www.w3.org/1999/xhtml': {
    global: ['title', 'aria-label', 'aria-valuetext', 'aria-moz-hint'],
    a: ['download'],
    area: ['download', 'alt'],
    // value is special-cased in isAttrNameAllowed
    input: ['alt', 'placeholder'],
    menuitem: ['label'],
    menu: ['label'],
    optgroup: ['label'],
    option: ['label'],
    track: ['label'],
    img: ['alt'],
    textarea: ['placeholder'],
    th: ['abbr']
  },
  'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul': {
    global: [
      'accesskey', 'aria-label', 'aria-valuetext', 'aria-moz-hint', 'label'
    ],
    key: ['key', 'keycode'],
    textbox: ['placeholder'],
    toolbarbutton: ['tooltiptext'],
  }
};


/**
 * Overlay translation onto a DOM element.
 *
 * @param   {Element} targetElement
 * @param   {string|Object} translation
 * @private
 */
function overlayElement(targetElement, translation) {
  const value = translation.value;

  if (typeof value === 'string') {
    if (!reOverlay.test(value)) {
      // If the translation doesn't contain any markup skip the overlay logic.
      targetElement.textContent = value;
    } else {
      // Else parse the translation's HTML using an inert template element,
      // sanitize it and replace the targetElement's content.
      const templateElement = targetElement.ownerDocument.createElementNS(
        'http://www.w3.org/1999/xhtml', 'template');
      templateElement.innerHTML = value;
      targetElement.appendChild(
        // The targetElement will be cleared at the end of sanitization.
        sanitizeUsing(templateElement.content, targetElement)
      );
    }
  }

  if (translation.attrs === null) {
    return;
  }

  const explicitlyAllowed = targetElement.hasAttribute('data-l10n-attrs')
    ? targetElement.getAttribute('data-l10n-attrs')
      .split(',').map(i => i.trim())
    : null;

  for (const [name, val] of translation.attrs) {
    if (isAttrNameAllowed(name, targetElement, explicitlyAllowed)) {
      targetElement.setAttribute(name, val);
    }
  }
}

/**
 * Sanitize `translationFragment` using `sourceElement` to add functional
 * HTML attributes to children.  `sourceElement` will have all its child nodes
 * removed.
 *
 * The sanitization is conducted according to the following rules:
 *
 *   - Allow text nodes.
 *   - Replace forbidden children with their textContent.
 *   - Remove forbidden attributes from allowed children.
 *
 * Additionally when a child of the same type is present in `sourceElement` its
 * attributes will be merged into the translated child.  Whitelisted attributes
 * of the translated child will then overwrite the ones present in the source.
 *
 * The overlay logic is subject to the following limitations:
 *
 *   - Children are always cloned.  Event handlers attached to them are lost.
 *   - Nested HTML in source and in translations is not supported.
 *   - Multiple children of the same type will be matched in order.
 *
 * @param {DocumentFragment} translationFragment
 * @param {Element} sourceElement
 * @private
 */
function sanitizeUsing(translationFragment, sourceElement) {
  // Take one node from translationFragment at a time and check it against
  // the allowed list or try to match it with a corresponding element
  // in the source.
  for (const childNode of translationFragment.childNodes) {

    if (childNode.nodeType === childNode.TEXT_NODE) {
      continue;
    }

    // If the child is forbidden just take its textContent.
    if (!isElementAllowed(childNode)) {
      const text = translationFragment.ownerDocument.createTextNode(
        childNode.textContent
      );
      translationFragment.replaceChild(text, childNode);
      continue;
    }


    // If a child of the same type exists in sourceElement, use it as the base
    // for the resultChild.  This also removes the child from sourceElement.
    const sourceChild = shiftNamedElement(sourceElement, childNode.localName);

    const mergedChild = sourceChild
      // Shallow-clone the sourceChild to remove all childNodes.
      ? sourceChild.cloneNode(false)
      // Create a fresh element as a way to remove all forbidden attributes.
      : childNode.ownerDocument.createElement(childNode.localName);

    // Explicitly discard nested HTML by serializing childNode to a TextNode.
    mergedChild.textContent = childNode.textContent;

    for (const attr of Array.from(childNode.attributes)) {
      if (isAttrNameAllowed(attr.name, childNode)) {
        mergedChild.setAttribute(attr.name, attr.value);
      }
    }

    translationFragment.replaceChild(mergedChild, childNode);
  }

  // SourceElement might have been already modified by shiftNamedElement.
  // Let's clear it to make sure other code doesn't rely on random leftovers.
  sourceElement.textContent = '';

  return translationFragment;
}

/**
 * Check if element is allowed in the translation.
 *
 * This method is used by the sanitizer when the translation markup contains
 * an element which is not present in the source code.
 *
 * @param   {Element} element
 * @returns {boolean}
 * @private
 */
function isElementAllowed(element) {
  const allowed = ALLOWED_ELEMENTS[element.namespaceURI];
  return allowed && allowed.includes(element.localName);
}

/**
 * Check if attribute is allowed for the given element.
 *
 * This method is used by the sanitizer when the translation markup contains
 * DOM attributes, or when the translation has traits which map to DOM
 * attributes.
 *
 * `explicitlyAllowed` can be passed as a list of attributes explicitly
 * allowed on this element.
 *
 * @param   {string}         name
 * @param   {Element}        element
 * @param   {Array}          explicitlyAllowed
 * @returns {boolean}
 * @private
 */
function isAttrNameAllowed(name, element, explicitlyAllowed = null) {
  if (explicitlyAllowed && explicitlyAllowed.includes(name)) {
    return true;
  }

  const allowed = ALLOWED_ATTRIBUTES[element.namespaceURI];
  if (!allowed) {
    return false;
  }

  const attrName = name.toLowerCase();
  const elemName = element.localName;

  // Is it a globally safe attribute?
  if (allowed.global.includes(attrName)) {
    return true;
  }

  // Are there no allowed attributes for this element?
  if (!allowed[elemName]) {
    return false;
  }

  // Is it allowed on this element?
  if (allowed[elemName].includes(attrName)) {
    return true;
  }

  // Special case for value on HTML inputs with type button, reset, submit
  if (element.namespaceURI === 'http://www.w3.org/1999/xhtml' &&
      elemName === 'input' && attrName === 'value') {
    const type = element.type.toLowerCase();
    if (type === 'submit' || type === 'button' || type === 'reset') {
      return true;
    }
  }

  return false;
}

/**
 * Remove and return the first child of the given type.
 *
 * @param {DOMFragment} element
 * @param {string}      localName
 * @returns {Element | null}
 * @private
 */
function shiftNamedElement(element, localName) {
  for (const child of element.children) {
    if (child.localName === localName) {
      element.removeChild(child);
      return child;
    }
  }
  return null;
}

const L10NID_ATTR_NAME = 'data-l10n-id';
const L10NARGS_ATTR_NAME = 'data-l10n-args';

const L10N_ELEMENT_QUERY = `[${L10NID_ATTR_NAME}]`;

/**
 * The `DOMLocalization` class is responsible for fetching resources and
 * formatting translations.
 *
 * It implements the fallback strategy in case of errors encountered during the
 * formatting of translations and methods for observing DOM
 * trees with a `MutationObserver`.
 */
class DOMLocalization extends Localization {
  /**
   * @param {Window}           windowElement
   * @param {Array<String>}    resourceIds      - List of resource IDs
   * @param {Function}         generateMessages - Function that returns a
   *                                              generator over MessageContexts
   * @returns {DOMLocalization}
   */
  constructor(windowElement, resourceIds, generateMessages) {
    super(resourceIds, generateMessages);

    // A Set of DOM trees observed by the `MutationObserver`.
    this.roots = new Set();
    // requestAnimationFrame handler.
    this.pendingrAF = null;
    // list of elements pending for translation.
    this.pendingElements = new Set();
    this.windowElement = windowElement;
    this.mutationObserver = new windowElement.MutationObserver(
      mutations => this.translateMutations(mutations)
    );

    this.observerConfig = {
      attribute: true,
      characterData: false,
      childList: true,
      subtree: true,
      attributeFilter: [L10NID_ATTR_NAME, L10NARGS_ATTR_NAME]
    };
  }

  onLanguageChange() {
    super.onLanguageChange();
    this.translateRoots();
  }

  /**
   * Set the `data-l10n-id` and `data-l10n-args` attributes on DOM elements.
   * FluentDOM makes use of mutation observers to detect changes
   * to `data-l10n-*` attributes and translate elements asynchronously.
   * `setAttributes` is a convenience method which allows to translate
   * DOM elements declaratively.
   *
   * You should always prefer to use `data-l10n-id` on elements (statically in
   * HTML or dynamically via `setAttributes`) over manually retrieving
   * translations with `format`.  The use of attributes ensures that the
   * elements can be retranslated when the user changes their language
   * preferences.
   *
   * ```javascript
   * localization.setAttributes(
   *   document.querySelector('#welcome'), 'hello', { who: 'world' }
   * );
   * ```
   *
   * This will set the following attributes on the `#welcome` element.
   * The MutationObserver will pick up this change and will localize the element
   * asynchronously.
   *
   * ```html
   * <p id='welcome'
   *   data-l10n-id='hello'
   *   data-l10n-args='{"who": "world"}'>
   * </p>
   * ```
   *
   * @param {Element}                element - Element to set attributes on
   * @param {string}                 id      - l10n-id string
   * @param {Object<string, string>} args    - KVP list of l10n arguments
   * @returns {Element}
   */
  setAttributes(element, id, args) {
    element.setAttribute(L10NID_ATTR_NAME, id);
    if (args) {
      element.setAttribute(L10NARGS_ATTR_NAME, JSON.stringify(args));
    } else {
      element.removeAttribute(L10NARGS_ATTR_NAME);
    }
    return element;
  }

  /**
   * Get the `data-l10n-*` attributes from DOM elements.
   *
   * ```javascript
   * localization.getAttributes(
   *   document.querySelector('#welcome')
   * );
   * // -> { id: 'hello', args: { who: 'world' } }
   * ```
   *
   * @param   {Element}  element - HTML element
   * @returns {{id: string, args: Object}}
   */
  getAttributes(element) {
    return {
      id: element.getAttribute(L10NID_ATTR_NAME),
      args: JSON.parse(element.getAttribute(L10NARGS_ATTR_NAME) || null)
    };
  }

  /**
   * Add `newRoot` to the list of roots managed by this `DOMLocalization`.
   *
   * Additionally, if this `DOMLocalization` has an observer, start observing
   * `newRoot` in order to translate mutations in it.
   *
   * @param {Element}      newRoot - Root to observe.
   */
  connectRoot(newRoot) {
    for (const root of this.roots) {
      if (root === newRoot ||
          root.contains(newRoot) ||
          newRoot.contains(root)) {
        throw new Error('Cannot add a root that overlaps with existing root.');
      }
    }

    this.roots.add(newRoot);
    this.mutationObserver.observe(newRoot, this.observerConfig);
  }

  /**
   * Remove `root` from the list of roots managed by this `DOMLocalization`.
   *
   * Additionally, if this `DOMLocalization` has an observer, stop observing
   * `root`.
   *
   * Returns `true` if the root was the last one managed by this
   * `DOMLocalization`.
   *
   * @param   {Element} root - Root to disconnect.
   * @returns {boolean}
   */
  disconnectRoot(root) {
    this.roots.delete(root);
    // Pause and resume the mutation observer to stop observing `root`.
    this.pauseObserving();
    this.resumeObserving();

    return this.roots.size === 0;
  }

  /**
   * Translate all roots associated with this `DOMLocalization`.
   *
   * @returns {Promise}
   */
  translateRoots() {
    const roots = Array.from(this.roots);
    return Promise.all(
      roots.map(root => this.translateElements(this.getTranslatables(root)))
    );
  }

  /**
   * Pauses the `MutationObserver`.
   *
   * @private
   */
  pauseObserving() {
    this.translateMutations(this.mutationObserver.takeRecords());
    this.mutationObserver.disconnect();
  }

  /**
   * Resumes the `MutationObserver`.
   *
   * @private
   */
  resumeObserving() {
    for (const root of this.roots) {
      this.mutationObserver.observe(root, this.observerConfig);
    }
  }

  /**
   * Translate mutations detected by the `MutationObserver`.
   *
   * @private
   */
  translateMutations(mutations) {
    for (const mutation of mutations) {
      switch (mutation.type) {
        case 'attributes':
          this.pendingElements.add(mutation.target);
          break;
        case 'childList':
          for (const addedNode of mutation.addedNodes) {
            if (addedNode.nodeType === addedNode.ELEMENT_NODE) {
              if (addedNode.childElementCount) {
                for (let element of this.getTranslatables(addedNode)) {
                  this.pendingElements.add(element);
                }
              } else if (addedNode.hasAttribute(L10NID_ATTR_NAME)) {
                this.pendingElements.add(addedNode);
              }
            }
          }
          break;
      }
    }

    // This fragment allows us to coalesce all pending translations into a single
    // requestAnimationFrame.
    if (this.pendingElements.size > 0) {
      if (this.pendingrAF === null) {
        this.pendingrAF = this.windowElement.requestAnimationFrame(() => {
          this.translateElements(Array.from(this.pendingElements));
          this.pendingElements.clear();
          this.pendingrAF = null;
        });
      }
    }
  }

  /**
   * Translate a DOM element or fragment asynchronously using this
   * `DOMLocalization` object.
   *
   * Manually trigger the translation (or re-translation) of a DOM fragment.
   * Use the `data-l10n-id` and `data-l10n-args` attributes to mark up the DOM
   * with information about which translations to use.
   *
   * Returns a `Promise` that gets resolved once the translation is complete.
   *
   * @param   {DOMFragment} frag - Element or DocumentFragment to be translated
   * @returns {Promise}
   */
  translateFragment(frag) {
    return this.translateElements(this.getTranslatables(frag));
  }

  /**
   * Translate a list of DOM elements asynchronously using this
   * `DOMLocalization` object.
   *
   * Manually trigger the translation (or re-translation) of a list of elements.
   * Use the `data-l10n-id` and `data-l10n-args` attributes to mark up the DOM
   * with information about which translations to use.
   *
   * Returns a `Promise` that gets resolved once the translation is complete.
   *
   * @param   {Array<Element>} elements - List of elements to be translated
   * @returns {Promise}
   */
  async translateElements(elements) {
    if (!elements.length) {
      return undefined;
    }

    const keys = elements.map(this.getKeysForElement);
    const translations = await this.formatMessages(keys);
    return this.applyTranslations(elements, translations);
  }

  /**
   * Applies translations onto elements.
   *
   * @param {Array<Element>} elements
   * @param {Array<Object>}  translations
   * @private
   */
  applyTranslations(elements, translations) {
    this.pauseObserving();

    for (let i = 0; i < elements.length; i++) {
      overlayElement(elements[i], translations[i]);
    }

    this.resumeObserving();
  }

  /**
   * Collects all translatable child elements of the element.
   *
   * @param {Element} element
   * @returns {Array<Element>}
   * @private
   */
  getTranslatables(element) {
    const nodes = Array.from(element.querySelectorAll(L10N_ELEMENT_QUERY));

    if (typeof element.hasAttribute === 'function' &&
        element.hasAttribute(L10NID_ATTR_NAME)) {
      nodes.push(element);
    }

    return nodes;
  }

  /**
   * Get the `data-l10n-*` attributes from DOM elements as a two-element
   * array.
   *
   * @param {Element} element
   * @returns {Array<string, Object>}
   * @private
   */
  getKeysForElement(element) {
    return [
      element.getAttribute(L10NID_ATTR_NAME),
      JSON.parse(element.getAttribute(L10NARGS_ATTR_NAME) || null)
    ];
  }
}

this.DOMLocalization = DOMLocalization;
this.EXPORTED_SYMBOLS = ['DOMLocalization'];
