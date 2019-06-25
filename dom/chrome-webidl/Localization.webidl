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

dictionary AttributeNameValue {
  required DOMString name;
  required DOMString value;
};

dictionary L10nMessage {
  DOMString? value = null;
  sequence<AttributeNameValue>? attributes = null;
};

callback GenerateMessages = Promise<any> (sequence<DOMString> aAppLocales, sequence<DOMString> aResourceIds);

/**
 * Localization is an implementation of the Fluent Localization API.
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
interface Localization {
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
  [NewObject] Promise<sequence<L10nMessage>> formatMessages(sequence<L10nKey> aKeys);
};