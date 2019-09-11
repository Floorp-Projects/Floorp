/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a Mozilla-specific WebExtension API, which is not available to web
 * content. It allows monitoring and filtering of HTTP response stream data.
 *
 * This API should currently be considered experimental, and is not defined by
 * any standard.
 */

[Func="mozilla::extensions::StreamFilter::IsAllowedInContext",
 Exposed=Window]
interface StreamFilterDataEvent : Event {
  constructor(DOMString type,
              optional StreamFilterDataEventInit eventInitDict = {});

  /**
   * Contains a chunk of data read from the input stream.
   */
  [Pure]
  readonly attribute ArrayBuffer data;
};

dictionary StreamFilterDataEventInit : EventInit {
  required ArrayBuffer data;
};

