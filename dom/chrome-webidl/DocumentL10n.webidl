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
[NoInterfaceObject,
 Exposed=Window]
interface DocumentL10n : DOMLocalization {
  /**
   * A promise which gets resolved when the initial DOM localization resources
   * fetching is complete and the initial translation of the DOM is finished.
   */
  readonly attribute Promise<any> ready;
};
