/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
[LegacyNoInterfaceObject,
 Exposed=Window]
interface DocumentL10n : DOMLocalization {
  /**
   * A promise which gets resolved when the initial DOM localization resources
   * fetching is complete and the initial translation of the DOM is finished.
   */
  readonly attribute Promise<any> ready;

  /**
   * An overload for the DOMLocalization::connectRoot which takes an optional second
   * argument to allow the user to express an intent of translating the root
   * as soon as the localization becomes available.
   *
   * If the root is being connected while the document is still being parsed,
   * then irrelevant of the value of the second argument, it will be translated
   * as part of the initial translation step right after the parsing completes.
   *
   * If the root is being connected after the document is parsed, then the
   * second argument controls whether the root is also going to get translated,
   * or just connected.
   *
   * This is a temporary workaround to avoid having to wait for the `DocumentL10n`
   * to become active. It should be unnecessary once we remove JSM and make
   * the `TranslateFragment` be available immediately when `DocumentL10n` becomes
   * available.
   */
  [Throws] void connectRoot(Node aElement, optional boolean aTranslate = false);
};
