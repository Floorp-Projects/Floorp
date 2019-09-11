/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback interface MozDocumentCallback {
  void onNewDocument(MozDocumentMatcher matcher, WindowProxy window);
  void onPreloadDocument(MozDocumentMatcher matcher, LoadInfo loadInfo);
};

[ChromeOnly, Exposed=Window]
interface MozDocumentObserver {
  constructor(MozDocumentCallback callbacks);

  [Throws]
  void observe(sequence<MozDocumentMatcher> matchers);
  void disconnect();
};
