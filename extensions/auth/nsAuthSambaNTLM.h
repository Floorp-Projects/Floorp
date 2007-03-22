/* vim:set ts=4 sw=4 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Samba NTLM Authentication.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan (rocallahan@novell.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
