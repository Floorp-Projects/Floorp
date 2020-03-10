/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly, Exposed=Window]
interface FluentResource {
  constructor(UTF8String source);
};

[ChromeOnly, Exposed=Window]
interface FluentPattern {};

/**
 * FluentMessage is a structure storing an unresolved L10nMessage,
 * as returned by the Fluent Bundle.
 *
 * It stores a FluentPattern of the value and attributes, which
 * can be then passed to bundle.formatPattern.
 */

dictionary FluentMessage {
  FluentPattern? value = null;
  required record<UTF8String, FluentPattern> attributes;
};
