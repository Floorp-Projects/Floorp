/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DOMLocalization is an extension of the Fluent Localization API.
 *
 * DOMLocalization adds a state for storing `roots` - DOM elements
 * which translation status is controlled by the DOMLocalization
 * instance and monitored for mutations.
 * DOMLocalization also adds methods dedicated to DOM manipulation.
 *
 * Methods:
 *    - connectRoot        - add a root
 *    - disconnectRoot     - remove a root
 *    - pauseObserving     - pause observing of roots
 *    - resumeObserving    - resume observing of roots
 *    - setAttributes      - set l10n attributes of an element
 *    - getAttributes      - retrieve l10n attributes of an element
 *    - translateFragment  - translate a DOM fragment
 *    - translateElements  - translate a list of DOM elements
 *    - translateRoots     - translate all attached roots
 *
 */

[ChromeOnly, Exposed=Window]
interface DOMLocalization : Localization {
  /**
   * Constructor arguments:
   *    - aResourceids       - a list of localization resource URIs
   *                           which will provide messages for this
   *                           Localization instance.
   *    - aSync              - Specifies if the initial state of the DOMLocalization
   *                           and the underlying Localization API is synchronous.
   *                           This enables a number of synchronous methods on the
   *                           Localization API and uses it for `TranslateElements`
   *                           making the method return a synchronusly resolved promise.
   *    - aBundleGenerator   - an object with two methods - `generateBundles` and
   *                           `generateBundlesSync` allowing consumers to overload the
   *                           default generators provided by Gecko.
   */
  [Throws]
  constructor(sequence<DOMString> aResourceIds,
              optional boolean aSync = false,
              optional BundleGenerator aBundleGenerator = {});

  /**
   * Adds a node to nodes observed for localization
   * related changes.
   */
  [Throws] void connectRoot(Node aElement);

  /**
   * Removes a node from nodes observed for localization
   * related changes.
   */
  [Throws] void disconnectRoot(Node aElement);

  /**
   * Pauses the MutationObserver set to observe
   * localization related DOM mutations.
   */
  [Throws] void pauseObserving();

  /**
   * Resumes the MutationObserver set to observe
   * localization related DOM mutations.
   */
  [Throws] void resumeObserving();

  /**
   * A helper function which allows the user to set localization-specific attributes
   * on an element.
   * This method lives on `document.l10n` for compatibility reasons with the
   * JS FluentDOM implementation. We may consider moving it onto Element.
   *
   * Example:
   *    document.l10n.setAttributes(h1, "key1", { emailCount: 5 });
   *
   *    <h1 data-l10n-id="key1" data-l10n-args="{\"emailCount\": 5}"/>
   */
  [Throws] void setAttributes(Element aElement, DOMString aId, optional object? aArgs);

  /**
   * A helper function which allows the user to retrieve a set of localization-specific
   * attributes from an element.
   * This method lives on `document.l10n` for compatibility reasons with the
   * JS FluentDOM implementation. We may consider moving it onto Element.
   *
   * Example:
   *    let l10nAttrs = document.l10n.getAttributes(h1);
   *    assert.deepEqual(l10nAttrs, {id: "key1", args: { emailCount: 5});
   */
  [Throws] L10nKey getAttributes(Element aElement);

  /**
   * Triggers translation of a subtree rooted at aNode.
   *
   * The method finds all translatable descendants of the argument and
   * localizes them.
   *
   * This method is mainly useful to trigger translation not covered by the
   * DOMLocalization's MutationObserver - for example before it gets attached
   * to the tree.
   * In such cases, when the already-translated fragment gets
   * injected into the observed root, one should `pauseObserving`,
   * then append the fragment, and finally `resumeObserving`.
   *
   * Example:
   *    await document.l10n.translatFragment(frag);
   *    root.pauseObserving();
   *    parent.appendChild(frag);
   *    root.resumeObserving();
   */
  [NewObject] Promise<any> translateFragment(Node aNode);

  /**
   * Triggers translation of a list of Elements using the localization context.
   *
   * The method only translates the elements directly passed to the method,
   * not any descendant nodes.
   *
   * This method is mainly useful to trigger translation not covered by the
   * DOMLocalization's MutationObserver - for example before it gets attached
   * to the tree.
   * In such cases, when the already-translated fragment gets
   * injected into the observed root, one should `pauseObserving`,
   * then append the fragment, and finally `resumeObserving`.
   *
   * Example:
   *    await document.l10n.translateElements([elem1, elem2]);
   *    root.pauseObserving();
   *    parent.appendChild(elem1);
   *    root.resumeObserving();
   *    alert(elem2.textContent);
   */
  [NewObject] Promise<void> translateElements(sequence<Element> aElements);

  /**
   * Triggers translation of all attached roots and sets their
   * locale info and directionality attributes.
   *
   * Example:
   *    await document.l10n.translateRoots();
   */
  [NewObject] Promise<void> translateRoots();
};
