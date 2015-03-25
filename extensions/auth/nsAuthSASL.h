/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAuthSASL_h__
#define nsAuthSASL_h__

#include "nsIAuthModule.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

/* This class is implemented using the nsAuthGSSAPI class, and the same
 * thread safety constraints which are documented in nsAuthGSSAPI.h
 * apply to this class
 */

class nsAuthSASL final : public nsIAuthModule
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIAUTHMODULE

    nsAuthSASL();

private:
    ~nsAuthSASL() { Reset(); }

    void Reset();

    nsCOMPtr<nsIAuthModule> mInnerModule;
    nsString       mUsername;
    bool           mSASLReady;
};

#endif /* nsAuthSASL_h__ */

