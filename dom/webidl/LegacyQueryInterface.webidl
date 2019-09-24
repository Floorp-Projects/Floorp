/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;

[Exposed=Window]
interface mixin LegacyQueryInterface {
  // Legacy QueryInterface, only exposed to chrome code on the main thread.
  [Exposed=Window, ChromeOnly]
  nsISupports QueryInterface(any iid);
};

Element includes LegacyQueryInterface;
