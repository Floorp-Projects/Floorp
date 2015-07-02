/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckPermissions="external-app"]
interface HTMLExtAppElement : HTMLElement {
  // Gets the value of the property from a property bag
  // that was provided to the external application.
  DOMString getCustomProperty(DOMString name);

  // Posts a message to the external application.
  [Throws]
  void postMessage(DOMString name);
};
