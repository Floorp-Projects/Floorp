/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSecurityMonitor_h
#define mozilla_dom_DOMSecurityMonitor_h

#include "nsStringFwd.h"

class nsIChannel;
class nsIPrincipal;

class DOMSecurityMonitor final {
 public:
  /* The fragment parser is triggered anytime JS calls innerHTML or similar
   * JS functions which can generate HTML fragments. This generation of
   * HTML might be dangerous, hence we should ensure that no new instances
   * of innerHTML and similar functions are introduced in system privileged
   * contexts, or also about: pages, in our codebase.
   *
   * If the auditor detects a new instance of innerHTML or similar
   * function it will CRASH using a strong assertion.
   */
  static void AuditParsingOfHTMLXMLFragments(nsIPrincipal* aPrincipal,
                                             const nsAString& aFragment);

  /* The use of javascript: URIs in system privileged contexts or
   * also about: pages is considered unsafe and discouraged.
   *
   * If the auditor detects a javascript: URI in a privileged
   * context it will CRASH using a strong assertion.
   *
   */
  static void AuditUseOfJavaScriptURI(nsIChannel* aChannel);

 private:
  DOMSecurityMonitor() = default;
  ~DOMSecurityMonitor() = default;
};

#endif /* mozilla_dom_DOMSecurityMonitor_h */
