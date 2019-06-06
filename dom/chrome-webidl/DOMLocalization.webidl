/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An object used to carry localization information from and to an
 * Element.
 *
 * Fields:
 *    id - identifier of a message used to localize the element.
 *  args - an optional map of arguments used to format the message.
 *         The argument will be converted to/from JSON, so can
 *         handle any value that JSON can, but in practice, the API
 *         will only use a string or a number for now.
 */
dictionary L10nKey {
  DOMString? id = null;
  object? args = null;
};

callback GenerateMessages = Promise<any> (sequence<DOMString> aAppLocales, sequence<DOMString> aResourceIds);

/**
 * DOMLocalization is an extension of the Fluent Localization API.
 *
 * An instance of a Localization class stores a state of a mix
 * of localization resources and provides the API to resolve
 * translation value for localization identifiers from the
 * resources.
 *
 * Methods:
 *    - addResourceIds     - add resources
 *    - removeResourceIds  - remove resources
 *    - formatValue        - format a single value
 *    - formatValues       - format multiple values
 *    - formatMessages     - format multiple compound messages
 *
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

/**
 * Constructor arguments:
 *    - aResourceids       - a list of localization resource URIs
 *                           which will provide messages for this
 *                           Localization instance.
 *    - aGenerateMessages  - a callback function which will be
 *                           used to generate an iterator
 *                           over FluentBundle instances.
 */
[ChromeOnly, Constructor(optional sequence<DOMString> aResourceIds, optional GenerateMessages aGenerateMessages)]
interface DOMLocalization {
  /**
   * Localization API
   */

  /**
   * A method for adding resources to the localization context.
   *
   * Returns a new count of resources used by the context.
   */
  unsigned long addResourceIds(sequence<DOMString> aResourceIds, optional boolean aEager = false);

  /**
   * A method for removing resources from the localization context.
   *
   * Returns a new count of resources used by the context.
   */
  unsigned long removeResourceIds(sequence<DOMString> aResourceIds);

  /**
   * Formats a value of a localization message with a given id.
   * An optional dictionary of arguments can be passed to inform
   * the message formatting logic.
   *
   * Example:
   *    let value = await document.l10n.formatValue("unread-emails", {count: 5});
   *    assert.equal(value, "You have 5 unread emails");
   */
  [NewObject] Promise<DOMString> formatValue(DOMString aId, optional object aArgs);

  /**
   * Formats values of a list of messages with given ids.
   *
   * Example:
   *    let values = await document.l10n.formatValues([
   *      {id: "hello-world"},
   *      {id: "unread-emails", args: {count: 5}
   *    ]);
   *    assert.deepEqual(values, [
   *      "Hello World",
   *      "You have 5 unread emails"
   *    ]);
   */
  [NewObject] Promise<sequence<DOMString>> formatValues(sequence<L10nKey> aKeys);

  /**
   * Formats values and attributes of a list of messages with given ids.
   *
   * Example:
   *    let values = await document.l10n.formatMessages([
   *      {id: "hello-world"},
   *      {id: "unread-emails", args: {count: 5}
   *    ]);
   *    assert.deepEqual(values, [
   *      {
   *        value: "Hello World",
   *        attributes: null
   *      },
   *      {
   *        value: "You have 5 unread emails",
   *        attributes: {
   *          tooltip: "Click to select them all"
   *        }
   *      }
   *    ]);
   */
  [NewObject] Promise<sequence<DOMString>> formatMessages(sequence<L10nKey> aKeys);


  /**
   * DOMLocalization API
   */

  /**
   * Adds a node to nodes observed for localization
   * related changes.
   */
  [Throws] void connectRoot(Element aElement);

  /**
   * Removes a node from nodes observed for localization
   * related changes.
   */
  [Throws] void disconnectRoot(Element aElement);

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
