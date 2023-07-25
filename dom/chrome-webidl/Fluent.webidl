/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary FluentTextElementItem {
  UTF8String id;
  UTF8String attr;
  UTF8String text;
};

[ChromeOnly, Exposed=Window]
interface FluentResource {
  constructor(UTF8String source);

  [Throws]
  sequence<FluentTextElementItem> textElements();
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

typedef record<UTF8String, (UTF8String or double)?> L10nArgs;

dictionary FluentBundleOptions {
  boolean useIsolating = false;
  UTF8String pseudoStrategy;
};

dictionary FluentBundleAddResourceOptions {
  boolean allowOverrides = false;
};

[ChromeOnly, Exposed=Window]
interface FluentBundle {
  [Throws]
  constructor((UTF8String or sequence<UTF8String>) aLocales, optional FluentBundleOptions aOptions = {});

  [Pure, Cached]
  readonly attribute sequence<UTF8String> locales;

  undefined addResource(FluentResource aResource, optional FluentBundleAddResourceOptions aOptions = {});
  boolean hasMessage(UTF8String id);
  FluentMessage? getMessage(UTF8String id);
  [Throws]
  UTF8String formatPattern(FluentPattern pattern, optional L10nArgs? aArgs = null, optional object aErrors);
};
