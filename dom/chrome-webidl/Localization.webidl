/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * L10nKey is an object used to carry localization tuple for message
 * translation.
 *
 * Fields:
 *    id - identifier of a message.
 *  args - an optional record of arguments used to format the message.
 *         The argument will be converted to/from JSON, and the API
 *         will only handle strings and numbers.
 */
dictionary L10nKey {
  UTF8String? id = null;
  L10nArgs? args = null;
};

/**
 * L10nMessage is a compound translation unit from Fluent which
 * encodes the value and (optionally) a list of attributes used
 * to translate a given widget.
 *
 * Most simple imperative translations will only use the `value`,
 * but when building a Message for a UI widget, a combination
 * of a value and attributes will be used.
 */
dictionary AttributeNameValue {
  required UTF8String name;
  required UTF8String value;
};

dictionary L10nMessage {
  UTF8String? value = null;
  sequence<AttributeNameValue>? attributes = null;
};

/**
 * A callback function which takes a list of localization resources
 * and produces an iterator over FluentBundle objects used for
 * localization with fallbacks.
 */
callback GenerateBundles = Promise<any> (sequence<DOMString> aResourceIds);
callback GenerateBundlesSync = any (sequence<DOMString> aResourceIds);

/**
 * The structure provides custom methods for the Localization API that
 * will be used to generate the `FluentBundle` iterator.
 * This allows the consumer to overload the default Gecko generator.
 */
dictionary BundleGenerator {
  GenerateBundles generateBundles;
  GenerateBundlesSync generateBundlesSync;
};

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
[ChromeOnly, Exposed=Window]
interface Localization {
  /**
   * Constructor arguments:
   *    - aResourceids         - a list of localization resource URIs
   *                             which will provide messages for this
   *                             Localization instance.
   *    - aSync                - Specifies if the initial state of the Localization API is synchronous.
   *                             This enables a number of synchronous methods on the
   *                             Localization API.
   *    - aBundleGenerator     - an object with two methods - `generateBundles` and
   *                             `generateBundlesSync` allowing consumers to overload the
   *                             default generators provided by Gecko.
   */
  [Throws]
  constructor(sequence<DOMString> aResourceIds,
              optional boolean aSync = false,
              optional BundleGenerator aBundleGenerator = {});

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
  [NewObject] Promise<UTF8String?> formatValue(UTF8String aId, optional L10nArgs aArgs);

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
  [NewObject] Promise<sequence<UTF8String?>> formatValues(sequence<L10nKey> aKeys);

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
  [NewObject] Promise<sequence<L10nMessage?>> formatMessages(sequence<L10nKey> aKeys);

  void setIsSync(boolean aIsSync);

  [NewObject, Throws]
  UTF8String? formatValueSync(UTF8String aId, optional L10nArgs aArgs);
  [NewObject, Throws]
  sequence<UTF8String?> formatValuesSync(sequence<L10nKey> aKeys);
  [NewObject, Throws]
  sequence<L10nMessage?> formatMessagesSync(sequence<L10nKey> aKeys);
};

/**
 * A helper dict for converting between JS Value and L10nArgs.
 */
[GenerateInitFromJSON, GenerateConversionToJS]
dictionary L10nArgsHelperDict {
  required L10nArgs args;
};
