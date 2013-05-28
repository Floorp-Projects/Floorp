/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// If this is changed, change the XPIDL dictionary as well.
dictionary MmsAttachment {
  DOMString? id = null;
  DOMString? location = null;
  Blob? content = null;
};

// If we start using MmsParameters here, remove it from DummyBinding.
