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

/* Based on fluent-dom@fa25466f (October 12, 2018) */
/* global DOMOverlays */

const { Localization } =
  ChromeUtils.import("resource://gre/modules/Localization.jsm");
const { Services } =
  ChromeUtils.import("resource://gre/modules/Services.jsm");

const L10NID_ATTR_NAME = "data-l10n-id";
const L10NARGS_ATTR_NAME = "data-l10n-args";

const L10N_ELEMENT_QUERY = `[${L10NID_ATTR_NAME}]`;

function reportDOMOverlayErrors(errors) {
  for (let error of errors) {
    switch (error.code) {
      case DOMOverlays.ERROR_FORBIDDEN_TYPE: {
        console.warn(
          `An element of forbidden type "${error.translatedElementName}" was found in ` +
          "the translation. Only safe text-level elements and elements with " +
          "data-l10n-name are allowed."
        );
        break;
      }
      case DOMOverlays.ERROR_NAMED_ELEMENT_MISSING: {
        console.warn(
          `An element named "${error.l10nName}" wasn't found in the source.`
        );
        break;
      }
      case DOMOverlays.ERROR_NAMED_ELEMENT_TYPE_MISMATCH: {
        console.warn(
          `An element named "${error.l10nName}" was found in the translation ` +
          `but its type ${error.translatedElementName} didn't match the ` +
          `element found in the source (${error.sourceElementName}).`
        );
        break;
      }
      default: {
        console.warn(`Unknown error ${error.code} happend while translation an element.`);
      }
    }
  }
}

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
   * @param {Array<String>}    resourceIds     - List of resource IDs
   * @param {Function}         generateBundles - Function that returns a
   *                                             generator over FluentBundles
   * @returns {DOMLocalization}
   */
  constructor(resourceIds, generateBundles) {
    super(resourceIds, generateBundles);

    // A Set of DOM trees observed by the `MutationObserver`.
    this.roots = new Set();
    // requestAnimationFrame handler.
    this.pendingrAF = null;
    // list of elements pending for translation.
    this.pendingElements = new Set();
    this.windowElement = null;
    this.mutationObserver = null;

    this.observerConfig = {
      attribute: true,
      characterData: false,
      childList: true,
      subtree: true,
      attributeFilter: [L10NID_ATTR_NAME, L10NARGS_ATTR_NAME],
    };
  }

  onChange(eager = false) {
    super.onChange(eager);
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
  setAttributes(element, id, args = null) {
    if (element.getAttribute(L10NID_ATTR_NAME) !== id) {
      element.setAttribute(L10NID_ATTR_NAME, id);
    }
    if (args) {
      let argsString = JSON.stringify(args);
      if (argsString !== element.getAttribute(L10NARGS_ATTR_NAME)) {
        element.setAttribute(L10NARGS_ATTR_NAME, argsString);
      }
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
      args: JSON.parse(element.getAttribute(L10NARGS_ATTR_NAME) || null),
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
    // Sometimes we connect the root while the document is already in the
    // process of being closed. Bail out gracefully.
    // See bug 1532712 for details.
    if (!newRoot.ownerGlobal) {
      return;
    }

    for (const root of this.roots) {
      if (root === newRoot ||
          root.contains(newRoot) ||
          newRoot.contains(root)) {
        throw new Error("Cannot add a root that overlaps with existing root.");
      }
    }

    if (this.windowElement) {
      if (this.windowElement !== newRoot.ownerGlobal) {
        throw new Error(`Cannot connect a root:
          DOMLocalization already has a root from a different window.`);
      }
    } else {
      this.windowElement = newRoot.ownerGlobal;
      this.mutationObserver = new this.windowElement.MutationObserver(
        mutations => this.translateMutations(mutations)
      );
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
    // Pause the mutation observer to stop observing `root`.
    this.pauseObserving();

    if (this.roots.size === 0) {
      this.mutationObserver = null;
      this.windowElement = null;
      this.pendingrAF = null;
      this.pendingElements.clear();
      return true;
    }

    // Resume observing all other roots.
    this.resumeObserving();
    return false;
  }

  /**
   * Translate all roots associated with this `DOMLocalization`.
   *
   * @returns {Promise}
   */
  translateRoots() {
    if (this.resourceIds.length === 0) {
      return Promise.resolve();
    }

    const roots = Array.from(this.roots);
    return Promise.all(
      roots.map(async root => {
        // We want to first retranslate the UI, and
        // then (potentially) flip the directionality.
        //
        // This means that the DOM alternations and directionality
        // are set in the same microtask.
        await this.translateFragment(root);
        let primaryLocale = Services.locale.appLocaleAsBCP47;
        let direction = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";
        root.setAttribute("lang", primaryLocale);
        root.setAttribute(root.namespaceURI ===
          "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
          ? "localedir" : "dir", direction);
      })
    );
  }

  /**
   * Pauses the `MutationObserver`.
   *
   * @private
   */
  pauseObserving() {
    if (!this.mutationObserver) {
      return;
    }

    this.translateMutations(this.mutationObserver.takeRecords());
    this.mutationObserver.disconnect();
  }

  /**
   * Resumes the `MutationObserver`.
   *
   * @private
   */
  resumeObserving() {
    if (!this.mutationObserver) {
      return;
    }

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
        case "attributes":
          if (mutation.target.hasAttribute("data-l10n-id")) {
            this.pendingElements.add(mutation.target);
          }
          break;
        case "childList":
          for (const addedNode of mutation.addedNodes) {
            if (addedNode.nodeType === addedNode.ELEMENT_NODE) {
              if (addedNode.childElementCount) {
                for (const element of this.getTranslatables(addedNode)) {
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

    // This fragment allows us to coalesce all pending translations
    // into a single requestAnimationFrame.
    if (this.pendingElements.size > 0) {
      if (this.pendingrAF === null) {
        this.pendingrAF = this.windowElement.requestAnimationFrame(() => {
          // We need to filter for elements that lost their l10n-id while
          // waiting for the animation frame.
          this.translateElements(Array.from(this.pendingElements)
            .filter(elem => elem.hasAttribute("data-l10n-id")));
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
  async translateFragment(frag) {
    if (frag.ownerDocument.l10n) {
      // We use DocumentL10n's version of this API.
      let errors = await frag.ownerDocument.l10n.translateFragment(frag);
      if (errors) {
        reportDOMOverlayErrors(errors);
      }
    } else {
      await this.translateElements(this.getTranslatables(frag));
    }
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

    // Remove elements from the pending list since
    // their translations will get applied below.
    for (let element of elements) {
      this.pendingElements.delete(element);
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

    const errors = [];
    for (let i = 0; i < elements.length; i++) {
      if (translations[i] !== undefined) {
        const translationErrors = DOMOverlays.translateElement(elements[i], translations[i]);
        if (translationErrors) {
          errors.push(...translationErrors);
        }
      }
    }
    if (errors.length) {
      reportDOMOverlayErrors(errors);
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

    if (typeof element.hasAttribute === "function" &&
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
   * @returns {Object}
   * @private
   */
  getKeysForElement(element) {
    return {
      id: element.getAttribute(L10NID_ATTR_NAME),
      args: JSON.parse(element.getAttribute(L10NARGS_ATTR_NAME) || null),
    };
  }
}

/**
 * Helper function which allows us to construct a new
 * DOMLocalization from DocumentL10n.
 */
var getDOMLocalization = () => new DOMLocalization();

var EXPORTED_SYMBOLS = ["DOMLocalization", "getDOMLocalization"];
