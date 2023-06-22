/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptElement_h
#define mozilla_dom_ScriptElement_h

#include "mozilla/Attributes.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIScriptElement.h"
#include "nsStubMutationObserver.h"

namespace mozilla::dom {

/**
 * Baseclass useful for script elements (such as <xhtml:script> and
 * <svg:script>). Currently the class assumes that only the 'src'
 * attribute and the children of the class affect what script to execute.
 */

class ScriptElement : public nsIScriptElement, public nsStubMutationObserver {
 public:
  // nsIScriptLoaderObserver
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED

  explicit ScriptElement(FromParser aFromParser)
      : nsIScriptElement(aFromParser) {}

  virtual nsresult FireErrorEvent() override;

  virtual bool GetScriptType(nsAString& aType) override;

 protected:
  // Internal methods

  /**
   * Check if this element contains any script, linked or inline
   */
  virtual bool HasScriptContent() = 0;

  virtual bool MaybeProcessScript() override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ScriptElement_h
