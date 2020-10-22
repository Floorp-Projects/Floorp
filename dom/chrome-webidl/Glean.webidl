/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;

[ChromeOnly, Exposed=Window]
interface GleanCategory {
  /**
   * Get a metric by name.
   *
   * Returns an object of the corresponding metric type,
   * with only the allowed functions available.
   */
  getter nsISupports (DOMString identifier);
};

[ChromeOnly, Exposed=Window]
interface GleanImpl {
  /**
   * Get a metric category by name.
   *
   * Returns an object for further metric lookup.
   */
  getter GleanCategory (DOMString identifier);
};
