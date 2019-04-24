/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
  required DOMString id;
  object? args;
};

/**
 * DocumentL10n is a public interface for handling DOM localization.
 *
 * In it's current form it exposes the DOMLocalization API which
 * allows for localization-specific DOM operations on a defined
 * localization context, and retrival of formatted localization
 * messages out of that context.
 *
 * The context is created when `<link rel="localization"/>` elements
 * are added to the document, and is removed in case all links
 * of that type are removed from it.
 */
[NoInterfaceObject]
interface DocumentL10n {
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
  [Throws] void setAttributes(Element aElement, DOMString aId, optional object aArgs);

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
   *
   * Example:
   *    await document.l10n.translatFragment(frag);
   *    parent.appendChild(frag);
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
   *
   * Example:
   *    await document.l10n.translateElements([elem1, elem2]);
   *    parent.appendChild(elem1);
   *    alert(elem2.textContent);
   */
  [NewObject] Promise<void> translateElements(sequence<Element> aElements);

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
   * A promise which gets resolved when the initial DOM localization resources
   * fetching is complete and the initial translation of the DOM is finished.
   */
  readonly attribute Promise<any> ready;
};
