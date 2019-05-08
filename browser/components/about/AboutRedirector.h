/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AboutRedirector_h__
#define AboutRedirector_h__

#include "nsIAboutModule.h"

namespace mozilla {
namespace browser {

class AboutRedirector : public nsIAboutModule {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIABOUTMODULE

  AboutRedirector() {}

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

 protected:
  virtual ~AboutRedirector() {}

 private:
  static bool sAboutLoginsEnabled;
  static bool sNewTabPageEnabled;
};

}  // namespace browser
}  // namespace mozilla

#endif  // AboutRedirector_h__
