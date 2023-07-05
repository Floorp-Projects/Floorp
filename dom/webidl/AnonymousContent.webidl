/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * This file declares the AnonymousContent interface which is used to
 * manipulate content that has been inserted into the document's canvasFrame
 * anonymous container. See Document.insertAnonymousContent.
 * Users of this API must never remove the host of the shadow root.
 */

[ChromeOnly, Exposed=Window]
interface AnonymousContent {
  readonly attribute ShadowRoot root;
};
