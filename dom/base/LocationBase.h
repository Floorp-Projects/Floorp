/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LocationBase_h
#define mozilla_dom_LocationBase_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsStringFwd.h"

class nsIDocShell;
class nsIPrincipal;
class nsIURI;
class nsDocShellLoadState;

namespace mozilla {
class ErrorResult;

namespace dom {
class BrowsingContext;

//*****************************************************************************
// Location: Script "location" object
//*****************************************************************************

class LocationBase {
 public:
  // WebIDL API:
  void Replace(const nsAString& aUrl, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetHref(const nsAString& aHref, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

 protected:
  virtual BrowsingContext* GetBrowsingContext() = 0;
  virtual nsIDocShell* GetDocShell() = 0;

  void SetURI(nsIURI* aURL, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv,
              bool aReplace = false);
  void SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                       nsIPrincipal& aSubjectPrincipal, bool aReplace,
                       ErrorResult& aRv);

  // Helper for Assign/SetHref/Replace
  void DoSetHref(const nsAString& aHref, nsIPrincipal& aSubjectPrincipal,
                 bool aReplace, ErrorResult& aRv);

  // Get the base URL we should be using for our relative URL
  // resolution for SetHref/Assign/Replace.
  nsIURI* GetSourceBaseURL();

  // Check whether it's OK to load the given url with the given subject
  // principal, and if so construct the right nsDocShellLoadInfo for the load
  // and return it.
  already_AddRefed<nsDocShellLoadState> CheckURL(
      nsIURI* url, nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_LocationBase_h
