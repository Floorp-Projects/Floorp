/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAuthSambaNTLM_h__
#define nsAuthSambaNTLM_h__

#include "nsIAuthModule.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "prio.h"
#include "prproces.h"

/**
 * This is an implementation of NTLM authentication that does single-signon
 * by obtaining the user's Unix username, parsing it into DOMAIN\name format,
 * and then asking Samba's ntlm_auth tool to do the authentication for us
 * using the user's password cached in winbindd, if available. If the
 * password is not available then this component fails to instantiate so
 * nsHttpNTLMAuth will fall back to a different NTLM implementation.
 * NOTE: at time of writing, this requires patches to be added to the stock
 * Samba winbindd and ntlm_auth!  
 */
class nsAuthSambaNTLM : public nsIAuthModule
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHMODULE

    nsAuthSambaNTLM();

    // We spawn the ntlm_auth helper from the module constructor, because
    // that lets us fail to instantiate the module if ntlm_auth isn't
    // available, triggering fallback to the built-in NTLM support (which
    // doesn't support single signon, of course)
    nsresult SpawnNTLMAuthHelper();

private:
    ~nsAuthSambaNTLM();

    void Shutdown();

    PRUint8*    mInitialMessage; /* free with free() */
    PRUint32    mInitialMessageLen;
    PRProcess*  mChildPID;
    PRFileDesc* mFromChildFD;
    PRFileDesc* mToChildFD;
};

#endif /* nsAuthSambaNTLM_h__ */
