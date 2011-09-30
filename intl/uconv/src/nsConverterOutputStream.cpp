/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
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
 * The Original Code is mozilla.org unicode stream converter code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

#include "nsIServiceManager.h"
#include "nsIOutputStream.h"
#include "nsICharsetConverterManager.h"

#include "nsConverterOutputStream.h"

NS_IMPL_ISUPPORTS2(nsConverterOutputStream,
                   nsIUnicharOutputStream,
                   nsIConverterOutputStream)

nsConverterOutputStream::~nsConverterOutputStream()
{
    Close();
}

NS_IMETHODIMP
nsConverterOutputStream::Init(nsIOutputStream* aOutStream,
                              const char*      aCharset,
                              PRUint32         aBufferSize /* ignored */,
                              PRUnichar        aReplacementChar)
{
    NS_PRECONDITION(aOutStream, "Null output stream!");

    if (!aCharset)
        aCharset = "UTF-8";

    nsresult rv;
    nsCOMPtr<nsICharsetConverterManager> ccm =
        do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = ccm->GetUnicodeEncoder(aCharset, getter_AddRefs(mConverter));
    if (NS_FAILED(rv))
        return rv;

    mOutStream = aOutStream;

    PRInt32 behaviour = aReplacementChar ? nsIUnicodeEncoder::kOnError_Replace
                                         : nsIUnicodeEncoder::kOnError_Signal;
    return mConverter->
        SetOutputErrorBehavior(behaviour,
                               nsnull,
                               aReplacementChar);
}

NS_IMETHODIMP
nsConverterOutputStream::Write(PRUint32 aCount, const PRUnichar* aChars,
                               bool* aSuccess)
{
    if (!mOutStream) {
        NS_ASSERTION(!mConverter, "Closed streams shouldn't have converters");
        return NS_BASE_STREAM_CLOSED;
    }
    NS_ASSERTION(mConverter, "Must have a converter when not closed");

    PRInt32 inLen = aCount;

    PRInt32 maxLen;
    nsresult rv = mConverter->GetMaxLength(aChars, inLen, &maxLen);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString buf;
    buf.SetLength(maxLen);
    if (buf.Length() != (PRUint32) maxLen)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 outLen = maxLen;
    rv = mConverter->Convert(aChars, &inLen, buf.BeginWriting(), &outLen);
    if (NS_FAILED(rv))
        return rv;
    if (rv == NS_ERROR_UENC_NOMAPPING) {
        // Yes, NS_ERROR_UENC_NOMAPPING is a success code
        return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
    }
    NS_ASSERTION((PRUint32) inLen == aCount,
                 "Converter didn't consume all the data!");

    PRUint32 written;
    rv = mOutStream->Write(buf.get(), outLen, &written);
    *aSuccess = NS_SUCCEEDED(rv) && written == PRUint32(outLen);
    return rv;

}

NS_IMETHODIMP
nsConverterOutputStream::WriteString(const nsAString& aString, bool* aSuccess)
{
    PRInt32 inLen = aString.Length();
    nsAString::const_iterator i;
    aString.BeginReading(i);
    return Write(inLen, i.get(), aSuccess);
}

NS_IMETHODIMP
nsConverterOutputStream::Flush()
{
    if (!mOutStream)
        return NS_OK; // Already closed.

    char buf[1024];
    PRInt32 size = sizeof(buf);
    nsresult rv = mConverter->Finish(buf, &size);
    NS_ASSERTION(rv != NS_OK_UENC_MOREOUTPUT,
                 "1024 bytes ought to be enough for everyone");
    if (NS_FAILED(rv))
        return rv;
    PRUint32 written;
    rv = mOutStream->Write(buf, size, &written);
    if (NS_FAILED(rv)) {
        NS_WARNING("Flush() lost data!");
        return rv;
    }
    if (written != PRUint32(size)) {
        NS_WARNING("Flush() lost data!");
        return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
    }
    return rv;
}

NS_IMETHODIMP
nsConverterOutputStream::Close()
{
    if (!mOutStream)
        return NS_OK; // Already closed.

    nsresult rv1 = Flush();

    nsresult rv2 = mOutStream->Close();
    mOutStream = nsnull;
    mConverter = nsnull;
    return NS_FAILED(rv1) ? rv1 : rv2;
}

