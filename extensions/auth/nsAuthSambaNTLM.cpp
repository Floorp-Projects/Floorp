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

#include "nsAuth.h"
#include "nsAuthSambaNTLM.h"
#include "prenv.h"
#include "plbase64.h"
#include "prerror.h"

#include <stdlib.h>

nsAuthSambaNTLM::nsAuthSambaNTLM()
    : mInitialMessage(nsnull), mChildPID(nsnull), mFromChildFD(nsnull),
      mToChildFD(nsnull)
{
}

nsAuthSambaNTLM::~nsAuthSambaNTLM()
{
    // ntlm_auth reads from stdin regularly so closing our file handles
    // should cause it to exit.
    Shutdown();
    free(mInitialMessage);
}

void
nsAuthSambaNTLM::Shutdown()
{
    if (mFromChildFD) {
        PR_Close(mFromChildFD);
        mFromChildFD = nsnull;
    }
    if (mToChildFD) {
        PR_Close(mToChildFD);
        mToChildFD = nsnull;
    }
    if (mChildPID) {
        PRInt32 exitCode;
        PR_WaitProcess(mChildPID, &exitCode);
        mChildPID = nsnull;
    }
}

NS_IMPL_ISUPPORTS1(nsAuthSambaNTLM, nsIAuthModule)

static bool
SpawnIOChild(char** aArgs, PRProcess** aPID,
             PRFileDesc** aFromChildFD, PRFileDesc** aToChildFD)
{
    PRFileDesc* toChildPipeRead;
    PRFileDesc* toChildPipeWrite;
    if (PR_CreatePipe(&toChildPipeRead, &toChildPipeWrite) != PR_SUCCESS)
        return PR_FALSE;
    PR_SetFDInheritable(toChildPipeRead, PR_TRUE);
    PR_SetFDInheritable(toChildPipeWrite, PR_FALSE);

    PRFileDesc* fromChildPipeRead;
    PRFileDesc* fromChildPipeWrite;
    if (PR_CreatePipe(&fromChildPipeRead, &fromChildPipeWrite) != PR_SUCCESS) {
        PR_Close(toChildPipeRead);
        PR_Close(toChildPipeWrite);
        return PR_FALSE;
    }
    PR_SetFDInheritable(fromChildPipeRead, PR_FALSE);
    PR_SetFDInheritable(fromChildPipeWrite, PR_TRUE);

    PRProcessAttr* attr = PR_NewProcessAttr();
    if (!attr) {
        PR_Close(fromChildPipeRead);
        PR_Close(fromChildPipeWrite);
        PR_Close(toChildPipeRead);
        PR_Close(toChildPipeWrite);
        return PR_FALSE;
    }

    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardInput, toChildPipeRead);
    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardOutput, fromChildPipeWrite);   

    PRProcess* process = PR_CreateProcess(aArgs[0], aArgs, nsnull, attr);
    PR_DestroyProcessAttr(attr);
    PR_Close(fromChildPipeWrite);
    PR_Close(toChildPipeRead);
    if (!process) {
        LOG(("ntlm_auth exec failure [%d]", PR_GetError()));
        PR_Close(fromChildPipeRead);
        PR_Close(toChildPipeWrite);
        return PR_FALSE;        
    }

    *aPID = process;
    *aFromChildFD = fromChildPipeRead;
    *aToChildFD = toChildPipeWrite;
    return PR_TRUE;
}

static bool WriteString(PRFileDesc* aFD, const nsACString& aString)
{
    PRInt32 length = aString.Length();
    const char* s = aString.BeginReading();
    LOG(("Writing to ntlm_auth: %s", s));

    while (length > 0) {
        int result = PR_Write(aFD, s, length);
        if (result <= 0)
            return PR_FALSE;
        s += result;
        length -= result;
    }
    return PR_TRUE;
}

static bool ReadLine(PRFileDesc* aFD, nsACString& aString)
{
    // ntlm_auth is defined to only send one line in response to each of our
    // input lines. So this simple unbuffered strategy works as long as we
    // read the response immediately after sending one request.
    aString.Truncate();
    for (;;) {
        char buf[1024];
        int result = PR_Read(aFD, buf, sizeof(buf));
        if (result <= 0)
            return PR_FALSE;
        aString.Append(buf, result);
        if (buf[result - 1] == '\n') {
            LOG(("Read from ntlm_auth: %s", nsPromiseFlatCString(aString).get()));
            return PR_TRUE;
        }
    }
}

/**
 * Returns a heap-allocated array of PRUint8s, and stores the length in aLen.
 * Returns nsnull if there's an error of any kind.
 */
static PRUint8* ExtractMessage(const nsACString& aLine, PRUint32* aLen)
{
    // ntlm_auth sends blobs to us as base64-encoded strings after the "xx "
    // preamble on the response line.
    PRInt32 length = aLine.Length();
    // The caller should verify there is a valid "xx " prefix and the line
    // is terminated with a \n
    NS_ASSERTION(length >= 4, "Line too short...");
    const char* line = aLine.BeginReading();
    const char* s = line + 3;
    length -= 4; // lose first 3 chars plus trailing \n
    NS_ASSERTION(s[length] == '\n', "aLine not newline-terminated");
    
    if (length & 3) {
        // The base64 encoded block must be multiple of 4. If not, something
        // screwed up.
        NS_WARNING("Base64 encoded block should be a multiple of 4 chars");
        return nsnull;
    } 

    // Calculate the exact length. I wonder why there isn't a function for this
    // in plbase64.
    PRInt32 numEquals;
    for (numEquals = 0; numEquals < length; ++numEquals) {
        if (s[length - 1 - numEquals] != '=')
            break;
    }
    *aLen = (length/4)*3 - numEquals;
    return reinterpret_cast<PRUint8*>(PL_Base64Decode(s, length, nsnull));
}

nsresult
nsAuthSambaNTLM::SpawnNTLMAuthHelper()
{
    const char* username = PR_GetEnv("USER");
    if (!username)
        return NS_ERROR_FAILURE;

    char* args[] = {
        "ntlm_auth",
        "--helper-protocol", "ntlmssp-client-1",
        "--use-cached-creds",
        "--username", const_cast<char*>(username),
        nsnull
    };

    bool isOK = SpawnIOChild(args, &mChildPID, &mFromChildFD, &mToChildFD);
    if (!isOK)  
        return NS_ERROR_FAILURE;

    if (!WriteString(mToChildFD, NS_LITERAL_CSTRING("YR\n")))
        return NS_ERROR_FAILURE;
    nsCString line;
    if (!ReadLine(mFromChildFD, line))
        return NS_ERROR_FAILURE;
    if (!StringBeginsWith(line, NS_LITERAL_CSTRING("YR "))) {
        // Something went wrong. Perhaps no credentials are accessible.
        return NS_ERROR_FAILURE;
    }

    // It gave us an initial client-to-server request packet. Save that
    // because we'll need it later.
    mInitialMessage = ExtractMessage(line, &mInitialMessageLen);
    if (!mInitialMessage)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Init(const char *serviceName,
                      PRUint32    serviceFlags,
                      const PRUnichar *domain,
                      const PRUnichar *username,
                      const PRUnichar *password)
{
    NS_ASSERTION(!username && !domain && !password, "unexpected credentials");
    return NS_OK;
}

NS_IMETHODIMP
nsAuthSambaNTLM::GetNextToken(const void *inToken,
                              PRUint32    inTokenLen,
                              void      **outToken,
                              PRUint32   *outTokenLen)
{
    if (!inToken) {
        /* someone wants our initial message */
        *outToken = nsMemory::Clone(mInitialMessage, mInitialMessageLen);
        if (!*outToken)
            return NS_ERROR_OUT_OF_MEMORY;
        *outTokenLen = mInitialMessageLen;
        return NS_OK;
    }

    /* inToken must be a type 2 message. Get ntlm_auth to generate our response */
    char* encoded = PL_Base64Encode(static_cast<const char*>(inToken), inTokenLen, nsnull);
    if (!encoded)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCString request;
    request.AssignLiteral("TT ");
    request.Append(encoded);
    free(encoded);
    request.Append('\n');

    if (!WriteString(mToChildFD, request))
        return NS_ERROR_FAILURE;
    nsCString line;
    if (!ReadLine(mFromChildFD, line))
        return NS_ERROR_FAILURE;
    if (!StringBeginsWith(line, NS_LITERAL_CSTRING("KK "))) {
        // Something went wrong. Perhaps no credentials are accessible.
        return NS_ERROR_FAILURE;
    }
    PRUint8* buf = ExtractMessage(line, outTokenLen);
    if (!buf)
        return NS_ERROR_FAILURE;
    // *outToken has to be freed by nsMemory::Free, which may not be free() 
    *outToken = nsMemory::Clone(buf, *outTokenLen);
    if (!*outToken) {
        free(buf);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    // We're done. Close our file descriptors now and reap the helper
    // process.
    Shutdown();
    return NS_SUCCESS_AUTH_FINISHED;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Unwrap(const void *inToken,
                        PRUint32    inTokenLen,
                        void      **outToken,
                        PRUint32   *outTokenLen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAuthSambaNTLM::Wrap(const void *inToken,
                      PRUint32    inTokenLen,
                      bool        confidential,
                      void      **outToken,
                      PRUint32   *outTokenLen)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
