/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsFormSubmission.h"

#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsDOMError.h"
#include "nsGenericHTMLElement.h"
#include "nsISaveAsCharset.h"
#include "nsIFile.h"
#include "nsIDOMFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsStringStream.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsLinebreakConverter.h"
#include "nsICharsetConverterManager.h"
#include "nsCharsetAlias.h"
#include "nsEscape.h"
#include "nsUnicharUtils.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsIMIMEService.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsCExternalHandlerService.h"
#include "nsIFileStreams.h"
#include "nsContentUtils.h"

using namespace mozilla;

static void
SendJSWarning(nsIDocument* aDocument,
              const char* aWarningName,
              const PRUnichar** aWarningArgs, PRUint32 aWarningArgsLen)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "HTML", aDocument,
                                  nsContentUtils::eFORMS_PROPERTIES,
                                  aWarningName,
                                  aWarningArgs, aWarningArgsLen);
}

// --------------------------------------------------------------------------

class nsFSURLEncoded : public nsEncodingFormSubmission
{
public:
  /**
   * @param aCharset the charset of the form as a string
   * @param aMethod the method of the submit (either NS_FORM_METHOD_GET or
   *        NS_FORM_METHOD_POST).
   */
  nsFSURLEncoded(const nsACString& aCharset,
                 PRInt32 aMethod,
                 nsIDocument* aDocument,
                 nsIContent* aOriginatingElement)
    : nsEncodingFormSubmission(aCharset, aOriginatingElement),
      mMethod(aMethod),
      mDocument(aDocument),
      mWarnedFileControl(false)
  {
  }

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue);
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   nsIDOMBlob* aBlob);
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream);

  virtual bool SupportsIsindexSubmission()
  {
    return true;
  }

  virtual nsresult AddIsindex(const nsAString& aValue);

protected:

  /**
   * URL encode a Unicode string by encoding it to bytes, converting linebreaks
   * properly, and then escaping many bytes as %xx.
   *
   * @param aStr the string to encode
   * @param aEncoded the encoded string [OUT]
   * @throws NS_ERROR_OUT_OF_MEMORY if we run out of memory
   */
  nsresult URLEncode(const nsAString& aStr, nsCString& aEncoded);

private:
  /**
   * The method of the submit (either NS_FORM_METHOD_GET or
   * NS_FORM_METHOD_POST).
   */
  PRInt32 mMethod;

  /** The query string so far (the part after the ?) */
  nsCString mQueryString;

  /** The document whose URI to use when reporting errors */
  nsCOMPtr<nsIDocument> mDocument;

  /** Whether or not we have warned about a file control not being submitted */
  bool mWarnedFileControl;
};

nsresult
nsFSURLEncoded::AddNameValuePair(const nsAString& aName,
                                 const nsAString& aValue)
{
  // Encode value
  nsCString convValue;
  nsresult rv = URLEncode(aValue, convValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Encode name
  nsCAutoString convName;
  rv = URLEncode(aName, convName);
  NS_ENSURE_SUCCESS(rv, rv);


  // Append data to string
  if (mQueryString.IsEmpty()) {
    mQueryString += convName + NS_LITERAL_CSTRING("=") + convValue;
  } else {
    mQueryString += NS_LITERAL_CSTRING("&") + convName
                  + NS_LITERAL_CSTRING("=") + convValue;
  }

  return NS_OK;
}

nsresult
nsFSURLEncoded::AddIsindex(const nsAString& aValue)
{
  // Encode value
  nsCString convValue;
  nsresult rv = URLEncode(aValue, convValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Append data to string
  if (mQueryString.IsEmpty()) {
    mQueryString.Assign(convValue);
  } else {
    mQueryString += NS_LITERAL_CSTRING("&isindex=") + convValue;
  }

  return NS_OK;
}

nsresult
nsFSURLEncoded::AddNameFilePair(const nsAString& aName,
                                nsIDOMBlob* aBlob)
{
  if (!mWarnedFileControl) {
    SendJSWarning(mDocument, "ForgotFileEnctypeWarning", nsnull, 0);
    mWarnedFileControl = true;
  }

  nsAutoString filename;
  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  if (file) {
    file->GetName(filename);
  }

  return AddNameValuePair(aName, filename);
}

static void
HandleMailtoSubject(nsCString& aPath) {

  // Walk through the string and see if we have a subject already.
  bool hasSubject = false;
  bool hasParams = false;
  PRInt32 paramSep = aPath.FindChar('?');
  while (paramSep != kNotFound && paramSep < (PRInt32)aPath.Length()) {
    hasParams = true;

    // Get the end of the name at the = op.  If it is *after* the next &,
    // assume that someone made a parameter without an = in it
    PRInt32 nameEnd = aPath.FindChar('=', paramSep+1);
    PRInt32 nextParamSep = aPath.FindChar('&', paramSep+1);
    if (nextParamSep == kNotFound) {
      nextParamSep = aPath.Length();
    }

    // If the = op is after the &, this parameter is a name without value.
    // If there is no = op, same thing.
    if (nameEnd == kNotFound || nextParamSep < nameEnd) {
      nameEnd = nextParamSep;
    }

    if (nameEnd != kNotFound) {
      if (Substring(aPath, paramSep+1, nameEnd-(paramSep+1)).
          LowerCaseEqualsLiteral("subject")) {
        hasSubject = true;
        break;
      }
    }

    paramSep = nextParamSep;
  }

  // If there is no subject, append a preformed subject to the mailto line
  if (!hasSubject) {
    if (hasParams) {
      aPath.Append('&');
    } else {
      aPath.Append('?');
    }

    // Get the default subject
    nsXPIDLString brandName;
    nsresult rv =
      nsContentUtils::GetLocalizedString(nsContentUtils::eBRAND_PROPERTIES,
                                         "brandShortName", brandName);
    if (NS_FAILED(rv))
      return;
    const PRUnichar *formatStrings[] = { brandName.get() };
    nsXPIDLString subjectStr;
    rv = nsContentUtils::FormatLocalizedString(
                                           nsContentUtils::eFORMS_PROPERTIES,
                                           "DefaultFormSubject",
                                           formatStrings,
                                           subjectStr);
    if (NS_FAILED(rv))
      return;
    aPath.AppendLiteral("subject=");
    nsCString subjectStrEscaped;
    aPath.Append(NS_EscapeURL(NS_ConvertUTF16toUTF8(subjectStr), esc_Query,
                              subjectStrEscaped));
  }
}

nsresult
nsFSURLEncoded::GetEncodedSubmission(nsIURI* aURI,
                                     nsIInputStream** aPostDataStream)
{
  nsresult rv = NS_OK;

  *aPostDataStream = nsnull;

  if (mMethod == NS_FORM_METHOD_POST) {

    bool isMailto = false;
    aURI->SchemeIs("mailto", &isMailto);
    if (isMailto) {

      nsCAutoString path;
      rv = aURI->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);

      HandleMailtoSubject(path);

      // Append the body to and force-plain-text args to the mailto line
      nsCString escapedBody;
      escapedBody.Adopt(nsEscape(mQueryString.get(), url_XAlphas));

      path += NS_LITERAL_CSTRING("&force-plain-text=Y&body=") + escapedBody;

      rv = aURI->SetPath(path);

    } else {

      nsCOMPtr<nsIInputStream> dataStream;
      // XXX We *really* need to either get the string to disown its data (and
      // not destroy it), or make a string input stream that owns the CString
      // that is passed to it.  Right now this operation does a copy.
      rv = NS_NewCStringInputStream(getter_AddRefs(dataStream), mQueryString);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIMIMEInputStream> mimeStream(
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
      NS_ENSURE_SUCCESS(rv, rv);

#ifdef SPECIFY_CHARSET_IN_CONTENT_TYPE
      mimeStream->AddHeader("Content-Type",
                            PromiseFlatString(
                              "application/x-www-form-urlencoded; charset="
                              + mCharset
                            ).get());
#else
      mimeStream->AddHeader("Content-Type",
                            "application/x-www-form-urlencoded");
#endif
      mimeStream->SetAddContentLength(true);
      mimeStream->SetData(dataStream);

      *aPostDataStream = mimeStream;
      NS_ADDREF(*aPostDataStream);
    }

  } else {
    // Get the full query string
    bool schemeIsJavaScript;
    rv = aURI->SchemeIs("javascript", &schemeIsJavaScript);
    NS_ENSURE_SUCCESS(rv, rv);
    if (schemeIsJavaScript) {
      return NS_OK;
    }

    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
    if (url) {
      url->SetQuery(mQueryString);
    }
    else {
      nsCAutoString path;
      rv = aURI->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);
      // Bug 42616: Trim off named anchor and save it to add later
      PRInt32 namedAnchorPos = path.FindChar('#');
      nsCAutoString namedAnchor;
      if (kNotFound != namedAnchorPos) {
        path.Right(namedAnchor, (path.Length() - namedAnchorPos));
        path.Truncate(namedAnchorPos);
      }

      // Chop off old query string (bug 25330, 57333)
      // Only do this for GET not POST (bug 41585)
      PRInt32 queryStart = path.FindChar('?');
      if (kNotFound != queryStart) {
        path.Truncate(queryStart);
      }

      path.Append('?');
      // Bug 42616: Add named anchor to end after query string
      path.Append(mQueryString + namedAnchor);

      aURI->SetPath(path);
    }
  }

  return rv;
}

// i18n helper routines
nsresult
nsFSURLEncoded::URLEncode(const nsAString& aStr, nsCString& aEncoded)
{
  // convert to CRLF breaks
  PRUnichar* convertedBuf =
    nsLinebreakConverter::ConvertUnicharLineBreaks(PromiseFlatString(aStr).get(),
                                                   nsLinebreakConverter::eLinebreakAny,
                                                   nsLinebreakConverter::eLinebreakNet);
  NS_ENSURE_TRUE(convertedBuf, NS_ERROR_OUT_OF_MEMORY);

  nsCAutoString encodedBuf;
  nsresult rv = EncodeVal(nsDependentString(convertedBuf), encodedBuf, false);
  nsMemory::Free(convertedBuf);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedBuf = nsEscape(encodedBuf.get(), url_XPAlphas);
  NS_ENSURE_TRUE(escapedBuf, NS_ERROR_OUT_OF_MEMORY);
  aEncoded.Adopt(escapedBuf);

  return NS_OK;
}

// --------------------------------------------------------------------------

nsFSMultipartFormData::nsFSMultipartFormData(const nsACString& aCharset,
                                             nsIContent* aOriginatingElement)
    : nsEncodingFormSubmission(aCharset, aOriginatingElement)
{
  mPostDataStream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  mBoundary.AssignLiteral("---------------------------");
  mBoundary.AppendInt(rand());
  mBoundary.AppendInt(rand());
  mBoundary.AppendInt(rand());
}

nsFSMultipartFormData::~nsFSMultipartFormData()
{
  NS_ASSERTION(mPostDataChunk.IsEmpty(), "Left unsubmitted data");
}

nsIInputStream*
nsFSMultipartFormData::GetSubmissionBody()
{
  // Finish data
  mPostDataChunk += NS_LITERAL_CSTRING("--") + mBoundary
                  + NS_LITERAL_CSTRING("--" CRLF);

  // Add final data input stream
  AddPostDataStream();

  return mPostDataStream;
}

nsresult
nsFSMultipartFormData::AddNameValuePair(const nsAString& aName,
                                        const nsAString& aValue)
{
  nsCString valueStr;
  nsCAutoString encodedVal;
  nsresult rv = EncodeVal(aValue, encodedVal, false);
  NS_ENSURE_SUCCESS(rv, rv);

  valueStr.Adopt(nsLinebreakConverter::
                 ConvertLineBreaks(encodedVal.get(),
                                   nsLinebreakConverter::eLinebreakAny,
                                   nsLinebreakConverter::eLinebreakNet));

  nsCAutoString nameStr;
  rv = EncodeVal(aName, nameStr, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make MIME block for name/value pair

  // XXX: name parameter should be encoded per RFC 2231
  // RFC 2388 specifies that RFC 2047 be used, but I think it's not 
  // consistent with MIME standard.
  mPostDataChunk += NS_LITERAL_CSTRING("--") + mBoundary
                 + NS_LITERAL_CSTRING(CRLF)
                 + NS_LITERAL_CSTRING("Content-Disposition: form-data; name=\"")
                 + nameStr + NS_LITERAL_CSTRING("\"" CRLF CRLF)
                 + valueStr + NS_LITERAL_CSTRING(CRLF);

  return NS_OK;
}

nsresult
nsFSMultipartFormData::AddNameFilePair(const nsAString& aName,
                                       nsIDOMBlob* aBlob)
{
  // Encode the control name
  nsCAutoString nameStr;
  nsresult rv = EncodeVal(aName, nameStr, true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString filename, contentType;
  nsCOMPtr<nsIInputStream> fileStream;
  if (aBlob) {
    // Get and encode the filename
    nsAutoString filename16;
    nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
    if (file) {
      rv = file->GetName(filename16);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (filename16.IsEmpty()) {
      filename16.AssignLiteral("blob");
    }

    rv = EncodeVal(filename16, filename, true);
    NS_ENSURE_SUCCESS(rv, rv);
  
    // Get content type
    nsAutoString contentType16;
    rv = aBlob->GetType(contentType16);
    if (NS_FAILED(rv) || contentType16.IsEmpty()) {
      contentType16.AssignLiteral("application/octet-stream");
    }
    contentType.Adopt(nsLinebreakConverter::
                      ConvertLineBreaks(NS_ConvertUTF16toUTF8(contentType16).get(),
                                        nsLinebreakConverter::eLinebreakAny,
                                        nsLinebreakConverter::eLinebreakSpace));
  
    // Get input stream
    rv = aBlob->GetInternalStream(getter_AddRefs(fileStream));
    NS_ENSURE_SUCCESS(rv, rv);
    if (fileStream) {
      // Create buffered stream (for efficiency)
      nsCOMPtr<nsIInputStream> bufferedStream;
      rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                     fileStream, 8192);
      NS_ENSURE_SUCCESS(rv, rv);
  
      fileStream = bufferedStream;
    }
  }
  else {
    contentType.AssignLiteral("application/octet-stream");
  }

  //
  // Make MIME block for name/value pair
  //
  // more appropriate than always using binary?
  mPostDataChunk += NS_LITERAL_CSTRING("--") + mBoundary
                 + NS_LITERAL_CSTRING(CRLF);
  // XXX: name/filename parameter should be encoded per RFC 2231
  // RFC 2388 specifies that RFC 2047 be used, but I think it's not 
  // consistent with the MIME standard.
  mPostDataChunk +=
         NS_LITERAL_CSTRING("Content-Disposition: form-data; name=\"")
       + nameStr + NS_LITERAL_CSTRING("\"; filename=\"")
       + filename + NS_LITERAL_CSTRING("\"" CRLF)
       + NS_LITERAL_CSTRING("Content-Type: ")
       + contentType + NS_LITERAL_CSTRING(CRLF CRLF);

  // Add the file to the stream
  if (fileStream) {
    // We need to dump the data up to this point into the POST data stream here,
    // since we're about to add the file input stream
    AddPostDataStream();

    mPostDataStream->AppendStream(fileStream);
  }

  // CRLF after file
  mPostDataChunk.AppendLiteral(CRLF);

  return NS_OK;
}

nsresult
nsFSMultipartFormData::GetEncodedSubmission(nsIURI* aURI,
                                            nsIInputStream** aPostDataStream)
{
  nsresult rv;

  // Make header
  nsCOMPtr<nsIMIMEInputStream> mimeStream
    = do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString contentType;
  GetContentType(contentType);
  mimeStream->AddHeader("Content-Type", contentType.get());
  mimeStream->SetAddContentLength(true);
  mimeStream->SetData(GetSubmissionBody());

  *aPostDataStream = mimeStream.forget().get();

  return NS_OK;
}

nsresult
nsFSMultipartFormData::AddPostDataStream()
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIInputStream> postDataChunkStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(postDataChunkStream),
                                mPostDataChunk);
  NS_ASSERTION(postDataChunkStream, "Could not open a stream for POST!");
  if (postDataChunkStream) {
    mPostDataStream->AppendStream(postDataChunkStream);
  }

  mPostDataChunk.Truncate();

  return rv;
}

// --------------------------------------------------------------------------

class nsFSTextPlain : public nsEncodingFormSubmission
{
public:
  nsFSTextPlain(const nsACString& aCharset, nsIContent* aOriginatingElement)
    : nsEncodingFormSubmission(aCharset, aOriginatingElement)
  {
  }

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue);
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   nsIDOMBlob* aBlob);
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream);

private:
  nsString mBody;
};

nsresult
nsFSTextPlain::AddNameValuePair(const nsAString& aName,
                                const nsAString& aValue)
{
  // XXX This won't work well with a name like "a=b" or "a\nb" but I suppose
  // text/plain doesn't care about that.  Parsers aren't built for escaped
  // values so we'll have to live with it.
  mBody.Append(aName + NS_LITERAL_STRING("=") + aValue +
               NS_LITERAL_STRING(CRLF));

  return NS_OK;
}

nsresult
nsFSTextPlain::AddNameFilePair(const nsAString& aName,
                               nsIDOMBlob* aBlob)
{
  nsAutoString filename;
  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(aBlob);
  if (file) {
    file->GetName(filename);
  }
    
  AddNameValuePair(aName, filename);
  return NS_OK;
}

nsresult
nsFSTextPlain::GetEncodedSubmission(nsIURI* aURI,
                                    nsIInputStream** aPostDataStream)
{
  nsresult rv = NS_OK;

  // XXX HACK We are using the standard URL mechanism to give the body to the
  // mailer instead of passing the post data stream to it, since that sounds
  // hard.
  bool isMailto = false;
  aURI->SchemeIs("mailto", &isMailto);
  if (isMailto) {
    nsCAutoString path;
    rv = aURI->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    HandleMailtoSubject(path);

    // Append the body to and force-plain-text args to the mailto line
    char* escapedBuf = nsEscape(NS_ConvertUTF16toUTF8(mBody).get(),
                                url_XAlphas);
    NS_ENSURE_TRUE(escapedBuf, NS_ERROR_OUT_OF_MEMORY);
    nsCString escapedBody;
    escapedBody.Adopt(escapedBuf);

    path += NS_LITERAL_CSTRING("&force-plain-text=Y&body=") + escapedBody;

    rv = aURI->SetPath(path);

  } else {
    // Create data stream.
    // We do want to send the data through the charset encoder and we want to
    // normalize linebreaks to use the "standard net" format (\r\n), but we
    // don't want to perform any other encoding. This means that names and
    // values which contains '=' or newlines are potentially ambigiously
    // encoded, but that how text/plain is specced.
    nsCString cbody;
    EncodeVal(mBody, cbody, false);
    cbody.Adopt(nsLinebreakConverter::
                ConvertLineBreaks(cbody.get(),
                                  nsLinebreakConverter::eLinebreakAny,
                                  nsLinebreakConverter::eLinebreakNet));
    nsCOMPtr<nsIInputStream> bodyStream;
    rv = NS_NewCStringInputStream(getter_AddRefs(bodyStream), cbody);
    if (!bodyStream) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Create mime stream with headers and such
    nsCOMPtr<nsIMIMEInputStream> mimeStream
        = do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mimeStream->AddHeader("Content-Type", "text/plain");
    mimeStream->SetAddContentLength(true);
    mimeStream->SetData(bodyStream);
    CallQueryInterface(mimeStream, aPostDataStream);
  }

  return rv;
}

// --------------------------------------------------------------------------

nsEncodingFormSubmission::nsEncodingFormSubmission(const nsACString& aCharset,
                                                   nsIContent* aOriginatingElement)
  : nsFormSubmission(aCharset, aOriginatingElement)
{
  nsCAutoString charset(aCharset);
  // canonical name is passed so that we just have to check against
  // *our* canonical names listed in charsetaliases.properties
  if (charset.EqualsLiteral("ISO-8859-1")) {
    charset.AssignLiteral("windows-1252");
  }

  if (!(charset.EqualsLiteral("UTF-8") || charset.EqualsLiteral("gb18030"))) {
    NS_ConvertUTF8toUTF16 charsetUtf16(charset);
    const PRUnichar* charsetPtr = charsetUtf16.get();
    SendJSWarning(aOriginatingElement ? aOriginatingElement->GetOwnerDocument()
                                      : nsnull,
                  "CannotEncodeAllUnicode",
                  &charsetPtr,
                  1);
  }

  mEncoder = do_CreateInstance(NS_SAVEASCHARSET_CONTRACTID);
  if (mEncoder) {
    nsresult rv =
      mEncoder->Init(charset.get(),
                     (nsISaveAsCharset::attr_EntityAfterCharsetConv + 
                      nsISaveAsCharset::attr_FallbackDecimalNCR),
                     0);
    if (NS_FAILED(rv)) {
      mEncoder = nsnull;
    }
  }
}

nsEncodingFormSubmission::~nsEncodingFormSubmission()
{
}

// i18n helper routines
nsresult
nsEncodingFormSubmission::EncodeVal(const nsAString& aStr, nsCString& aOut,
                                    bool aHeaderEncode)
{
  if (mEncoder && !aStr.IsEmpty()) {
    aOut.Truncate();
    nsresult rv = mEncoder->Convert(PromiseFlatString(aStr).get(),
                                    getter_Copies(aOut));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // fall back to UTF-8
    CopyUTF16toUTF8(aStr, aOut);
  }

  if (aHeaderEncode) {
    aOut.Adopt(nsLinebreakConverter::
               ConvertLineBreaks(aOut.get(),
                                 nsLinebreakConverter::eLinebreakAny,
                                 nsLinebreakConverter::eLinebreakSpace));
    aOut.ReplaceSubstring(NS_LITERAL_CSTRING("\""),
                          NS_LITERAL_CSTRING("\\\""));
  }


  return NS_OK;
}

// --------------------------------------------------------------------------

static void
GetSubmitCharset(nsGenericHTMLElement* aForm,
                 nsACString& oCharset)
{
  oCharset.AssignLiteral("UTF-8"); // default to utf-8

  nsAutoString acceptCharsetValue;
  aForm->GetAttr(kNameSpaceID_None, nsGkAtoms::acceptcharset,
                 acceptCharsetValue);

  PRInt32 charsetLen = acceptCharsetValue.Length();
  if (charsetLen > 0) {
    PRInt32 offset=0;
    PRInt32 spPos=0;
    // get charset from charsets one by one
    do {
      spPos = acceptCharsetValue.FindChar(PRUnichar(' '), offset);
      PRInt32 cnt = ((-1==spPos)?(charsetLen-offset):(spPos-offset));
      if (cnt > 0) {
        nsAutoString uCharset;
        acceptCharsetValue.Mid(uCharset, offset, cnt);

        if (NS_SUCCEEDED(nsCharsetAlias::GetPreferred(NS_LossyConvertUTF16toASCII(uCharset),
                                                      oCharset)))
          return;
      }
      offset = spPos + 1;
    } while (spPos != -1);
  }
  // if there are no accept-charset or all the charset are not supported
  // Get the charset from document
  nsIDocument* doc = aForm->GetDocument();
  if (doc) {
    oCharset = doc->GetDocumentCharacterSet();
  }
}

static void
GetEnumAttr(nsGenericHTMLElement* aContent,
            nsIAtom* atom, PRInt32* aValue)
{
  const nsAttrValue* value = aContent->GetParsedAttr(atom);
  if (value && value->Type() == nsAttrValue::eEnum) {
    *aValue = value->GetEnumValue();
  }
}

nsresult
GetSubmissionFromForm(nsGenericHTMLElement* aForm,
                      nsGenericHTMLElement* aOriginatingElement,
                      nsFormSubmission** aFormSubmission)
{
  // Get all the information necessary to encode the form data
  NS_ASSERTION(aForm->GetCurrentDoc(),
               "Should have doc if we're building submission!");

  // Get encoding type (default: urlencoded)
  PRInt32 enctype = NS_FORM_ENCTYPE_URLENCODED;
  if (aOriginatingElement &&
      aOriginatingElement->HasAttr(kNameSpaceID_None, nsGkAtoms::formenctype)) {
    GetEnumAttr(aOriginatingElement, nsGkAtoms::formenctype, &enctype);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::enctype, &enctype);
  }

  // Get method (default: GET)
  PRInt32 method = NS_FORM_METHOD_GET;
  if (aOriginatingElement &&
      aOriginatingElement->HasAttr(kNameSpaceID_None, nsGkAtoms::formmethod)) {
    GetEnumAttr(aOriginatingElement, nsGkAtoms::formmethod, &method);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::method, &method);
  }

  // Get charset
  nsCAutoString charset;
  GetSubmitCharset(aForm, charset);

  // We now have a canonical charset name, so we only have to check it
  // against canonical names.

  // use UTF-8 for UTF-16* (per WHATWG and existing practice of
  // MS IE/Opera).
  if (StringBeginsWith(charset, NS_LITERAL_CSTRING("UTF-16"))) {
    charset.AssignLiteral("UTF-8");
  }

  // Choose encoder
  if (method == NS_FORM_METHOD_POST &&
      enctype == NS_FORM_ENCTYPE_MULTIPART) {
    *aFormSubmission = new nsFSMultipartFormData(charset, aOriginatingElement);
  } else if (method == NS_FORM_METHOD_POST &&
             enctype == NS_FORM_ENCTYPE_TEXTPLAIN) {
    *aFormSubmission = new nsFSTextPlain(charset, aOriginatingElement);
  } else {
    nsIDocument* doc = aForm->OwnerDoc();
    if (enctype == NS_FORM_ENCTYPE_MULTIPART ||
        enctype == NS_FORM_ENCTYPE_TEXTPLAIN) {
      nsAutoString enctypeStr;
      if (aOriginatingElement &&
          aOriginatingElement->HasAttr(kNameSpaceID_None,
                                       nsGkAtoms::formenctype)) {
        aOriginatingElement->GetAttr(kNameSpaceID_None, nsGkAtoms::formenctype,
                                     enctypeStr);
      } else {
        aForm->GetAttr(kNameSpaceID_None, nsGkAtoms::enctype, enctypeStr);
      }
      const PRUnichar* enctypeStrPtr = enctypeStr.get();
      SendJSWarning(doc, "ForgotPostWarning",
                    &enctypeStrPtr, 1);
    }
    *aFormSubmission = new nsFSURLEncoded(charset, method, doc,
                                          aOriginatingElement);
  }
  NS_ENSURE_TRUE(*aFormSubmission, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}
