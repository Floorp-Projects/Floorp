/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"

#include "nsIOutputStream.h"
#include "nsString.h"

#include "nsConverterOutputStream.h"
#include "nsIUnicodeEncoder.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

NS_IMPL_ISUPPORTS(nsConverterOutputStream,
                  nsIUnicharOutputStream,
                  nsIConverterOutputStream)

nsConverterOutputStream::~nsConverterOutputStream()
{
    Close();
}

NS_IMETHODIMP
nsConverterOutputStream::Init(nsIOutputStream* aOutStream,
                              const char*      aCharset,
                              uint32_t         aBufferSize /* ignored */,
                              char16_t        aReplacementChar)
{
    NS_PRECONDITION(aOutStream, "Null output stream!");

    nsAutoCString label;
    if (!aCharset) {
        label.AssignLiteral("UTF-8");
    } else {
        label = aCharset;
    }

    nsAutoCString encoding;
    if (label.EqualsLiteral("UTF-16")) {
        // Make sure to output a BOM when UTF-16 requested
        encoding.Assign(label);
    } else if (!EncodingUtils::FindEncodingForLabelNoReplacement(label,
                                                                 encoding)) {
      return NS_ERROR_UCONV_NOCONV;
    }
    mConverter = EncodingUtils::EncoderForEncoding(encoding);

    mOutStream = aOutStream;

    int32_t behaviour = aReplacementChar ? nsIUnicodeEncoder::kOnError_Replace
                                         : nsIUnicodeEncoder::kOnError_Signal;
    return mConverter->
        SetOutputErrorBehavior(behaviour,
                               nullptr,
                               aReplacementChar);
}

NS_IMETHODIMP
nsConverterOutputStream::Write(uint32_t aCount, const char16_t* aChars,
                               bool* aSuccess)
{
    if (!mOutStream) {
        NS_ASSERTION(!mConverter, "Closed streams shouldn't have converters");
        return NS_BASE_STREAM_CLOSED;
    }
    NS_ASSERTION(mConverter, "Must have a converter when not closed");

    int32_t inLen = aCount;

    int32_t maxLen;
    nsresult rv = mConverter->GetMaxLength(aChars, inLen, &maxLen);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString buf;
    buf.SetLength(maxLen);
    if (buf.Length() != (uint32_t) maxLen)
        return NS_ERROR_OUT_OF_MEMORY;

    int32_t outLen = maxLen;
    rv = mConverter->Convert(aChars, &inLen, buf.BeginWriting(), &outLen);
    if (NS_FAILED(rv))
        return rv;
    if (rv == NS_ERROR_UENC_NOMAPPING) {
        // Yes, NS_ERROR_UENC_NOMAPPING is a success code
        return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
    }
    NS_ASSERTION((uint32_t) inLen == aCount,
                 "Converter didn't consume all the data!");

    uint32_t written;
    rv = mOutStream->Write(buf.get(), outLen, &written);
    *aSuccess = NS_SUCCEEDED(rv) && written == uint32_t(outLen);
    return rv;

}

NS_IMETHODIMP
nsConverterOutputStream::WriteString(const nsAString& aString, bool* aSuccess)
{
    int32_t inLen = aString.Length();
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
    int32_t size = sizeof(buf);
    nsresult rv = mConverter->Finish(buf, &size);
    NS_ASSERTION(rv != NS_OK_UENC_MOREOUTPUT,
                 "1024 bytes ought to be enough for everyone");
    if (NS_FAILED(rv))
        return rv;
    if (size == 0)
        return NS_OK;

    uint32_t written;
    rv = mOutStream->Write(buf, size, &written);
    if (NS_FAILED(rv)) {
        NS_WARNING("Flush() lost data!");
        return rv;
    }
    if (written != uint32_t(size)) {
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
    mOutStream = nullptr;
    mConverter = nullptr;
    return NS_FAILED(rv1) ? rv1 : rv2;
}

