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
 * The Original Code is saslgssapi
 *
 * The Initial Developer of the Original Code is Simon Wilkinson
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Wilkinson <simon@sxw.org.uk>
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

#include "nsComponentManagerUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"

#include "nsAuthSASL.h"

static const char kNegotiateAuthSSPI[] = "network.auth.use-sspi";

nsAuthSASL::nsAuthSASL()
{
    mSASLReady = false;
}

void nsAuthSASL::Reset() 
{
    mSASLReady = false;
}

/* Limitations apply to this class's thread safety. See the header file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsAuthSASL, nsIAuthModule)

NS_IMETHODIMP
nsAuthSASL::Init(const char *serviceName,
                 PRUint32    serviceFlags,
                 const PRUnichar *domain,
                 const PRUnichar *username,
                 const PRUnichar *password)
{
    nsresult rv;
    
    NS_ASSERTION(username, "SASL requires a username");
    NS_ASSERTION(!domain && !password, "unexpected credentials");

    mUsername = username;
    
    // If we're doing SASL, we should do mutual auth
    serviceFlags |= REQ_MUTUAL_AUTH;
   
    // Find out whether we should be trying SSPI or not
    const char *contractID = NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-gss";
    
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
        bool val;
        rv = prefs->GetBoolPref(kNegotiateAuthSSPI, &val);
        if (NS_SUCCEEDED(rv) && val)
            contractID = NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-sspi";
    }
    
    mInnerModule = do_CreateInstance(contractID, &rv);
    // if we can't create the GSSAPI module, then bail
    NS_ENSURE_SUCCESS(rv, rv);

    mInnerModule->Init(serviceName, serviceFlags, nsnull, nsnull, nsnull);

    return NS_OK;
}

NS_IMETHODIMP
nsAuthSASL::GetNextToken(const void *inToken,
                         PRUint32    inTokenLen,
                         void      **outToken,
                         PRUint32   *outTokenLen)
{
    nsresult rv;
    void *unwrappedToken;
    char *message;
    PRUint32 unwrappedTokenLen, messageLen;
    nsCAutoString userbuf;
    
    if (!mInnerModule) 
        return NS_ERROR_NOT_INITIALIZED;

    if (mSASLReady) {
        // If the server COMPLETEs with an empty token, Cyrus sends us that token.
        // I don't think this is correct, but we need to handle that behaviour.
        // Cyrus ignores the contents of our reply token.
        if (inTokenLen == 0) {
            *outToken = NULL;
            *outTokenLen = 0;
            return NS_OK;
        }
        // We've completed the GSSAPI portion of the handshake, and are
        // now ready to do the SASL security layer and authzid negotiation

        // Input packet from the server needs to be unwrapped.
        rv = mInnerModule->Unwrap(inToken, inTokenLen, &unwrappedToken, 
                                  &unwrappedTokenLen);
        if (NS_FAILED(rv)) {
            Reset();
            return rv;
        }
        
        // If we were doing security layers then we'd care what the
        // server had sent us. We're not, so all we had to do was make
        // sure that the signature was correct with the above unwrap()
        nsMemory::Free(unwrappedToken);
        
        NS_CopyUnicodeToNative(mUsername, userbuf);
        messageLen = userbuf.Length() + 4 + 1;
        message = (char *)nsMemory::Alloc(messageLen);
        if (!message) {
          Reset();
          return NS_ERROR_OUT_OF_MEMORY;
        }
        message[0] = 0x01; // No security layer
        message[1] = 0x00;
        message[2] = 0x00;
        message[3] = 0x00; // Maxbuf must be zero if we've got no sec layer
        strcpy(message+4, userbuf.get());
        // Userbuf should not be NULL terminated, so trim the trailing NULL
        // when wrapping the message
        rv = mInnerModule->Wrap((void *) message, messageLen-1, PR_FALSE, 
                                outToken, outTokenLen);
        nsMemory::Free(message);
        Reset(); // All done
        return NS_SUCCEEDED(rv) ? NS_SUCCESS_AUTH_FINISHED : rv;
    }
    rv = mInnerModule->GetNextToken(inToken, inTokenLen, outToken, 
                                    outTokenLen);
    if (rv == NS_SUCCESS_AUTH_FINISHED) {
        mSASLReady = true;
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
nsAuthSASL::Unwrap(const void *inToken,
                   PRUint32    inTokenLen,
                   void      **outToken,
                   PRUint32   *outTokenLen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAuthSASL::Wrap(const void *inToken,
                 PRUint32    inTokenLen,
                 bool        confidential,
                 void      **outToken,
                 PRUint32   *outTokenLen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
