/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    if (size == 0)
        return NS_OK;

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

