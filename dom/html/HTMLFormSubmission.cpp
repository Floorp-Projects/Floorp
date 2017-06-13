/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFormSubmission.h"

#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsISaveAsCharset.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsStringStream.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsLinebreakConverter.h"
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
#include "mozilla/Telemetry.h"

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

namespace {

void
SendJSWarning(nsIDocument* aDocument,
              const char* aWarningName,
              const char16_t** aWarningArgs, uint32_t aWarningArgsLen)
{
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("HTML"), aDocument,
                                  nsContentUtils::eFORMS_PROPERTIES,
                                  aWarningName,
                                  aWarningArgs, aWarningArgsLen);
}

void
RetrieveFileName(Blob* aBlob, nsAString& aFilename)
{
  if (!aBlob) {
    return;
  }

  RefPtr<File> file = aBlob->ToFile();
  if (file) {
    file->GetName(aFilename);
  }
}

void
RetrieveDirectoryName(Directory* aDirectory, nsAString& aDirname)
{
  MOZ_ASSERT(aDirectory);

  ErrorResult rv;
  aDirectory->GetName(aDirname, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    aDirname.Truncate();
  }
}

// --------------------------------------------------------------------------

class FSURLEncoded : public EncodingFormSubmission
{
public:
  /**
   * @param aEncoding the character encoding of the form
   * @param aMethod the method of the submit (either NS_FORM_METHOD_GET or
   *        NS_FORM_METHOD_POST).
   */
  FSURLEncoded(NotNull<const Encoding*> aEncoding,
               int32_t aMethod,
               nsIDocument* aDocument,
               nsIContent* aOriginatingElement)
    : EncodingFormSubmission(aEncoding, aOriginatingElement)
    , mMethod(aMethod)
    , mDocument(aDocument)
    , mWarnedFileControl(false)
  {
  }

  virtual nsresult
  AddNameValuePair(const nsAString& aName, const nsAString& aValue) override;

  virtual nsresult
  AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob) override;

  virtual nsresult
  AddNameDirectoryPair(const nsAString& aName, Directory* aDirectory) override;

  virtual nsresult
  GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream) override;

  virtual bool SupportsIsindexSubmission() override
  {
    return true;
  }

  virtual nsresult AddIsindex(const nsAString& aValue) override;

protected:

  /**
   * URL encode a Unicode string by encoding it to bytes, converting linebreaks
   * properly, and then escaping many bytes as %xx.
   *
   * @param aStr the string to encode
   * @param aEncoded the encoded string [OUT]
   * @throws NS_ERROR_OUT_OF_MEMORY if we run out of memory
   */
  nsresult URLEncode(const nsAString& aStr, nsACString& aEncoded);

private:
  /**
   * The method of the submit (either NS_FORM_METHOD_GET or
   * NS_FORM_METHOD_POST).
   */
  int32_t mMethod;

  /** The query string so far (the part after the ?) */
  nsCString mQueryString;

  /** The document whose URI to use when reporting errors */
  nsCOMPtr<nsIDocument> mDocument;

  /** Whether or not we have warned about a file control not being submitted */
  bool mWarnedFileControl;
};

nsresult
FSURLEncoded::AddNameValuePair(const nsAString& aName,
                               const nsAString& aValue)
{
  // Encode value
  nsCString convValue;
  nsresult rv = URLEncode(aValue, convValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Encode name
  nsAutoCString convName;
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
FSURLEncoded::AddIsindex(const nsAString& aValue)
{
  // Encode value
  nsCString convValue;
  nsresult rv = URLEncode(aValue, convValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Append data to string
  if (mQueryString.IsEmpty()) {
    Telemetry::Accumulate(Telemetry::FORM_ISINDEX_USED, true);
    mQueryString.Assign(convValue);
  } else {
    mQueryString += NS_LITERAL_CSTRING("&isindex=") + convValue;
  }

  return NS_OK;
}

nsresult
FSURLEncoded::AddNameBlobOrNullPair(const nsAString& aName,
                                    Blob* aBlob)
{
  if (!mWarnedFileControl) {
    SendJSWarning(mDocument, "ForgotFileEnctypeWarning", nullptr, 0);
    mWarnedFileControl = true;
  }

  nsAutoString filename;
  RetrieveFileName(aBlob, filename);
  return AddNameValuePair(aName, filename);
}

nsresult
FSURLEncoded::AddNameDirectoryPair(const nsAString& aName,
                                   Directory* aDirectory)
{
  // No warning about because Directory objects are never sent via form.

  nsAutoString dirname;
  RetrieveDirectoryName(aDirectory, dirname);
  return AddNameValuePair(aName, dirname);
}

void
HandleMailtoSubject(nsCString& aPath)
{
  // Walk through the string and see if we have a subject already.
  bool hasSubject = false;
  bool hasParams = false;
  int32_t paramSep = aPath.FindChar('?');
  while (paramSep != kNotFound && paramSep < (int32_t)aPath.Length()) {
    hasParams = true;

    // Get the end of the name at the = op.  If it is *after* the next &,
    // assume that someone made a parameter without an = in it
    int32_t nameEnd = aPath.FindChar('=', paramSep+1);
    int32_t nextParamSep = aPath.FindChar('&', paramSep+1);
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
    const char16_t *formatStrings[] = { brandName.get() };
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
    rv = NS_EscapeURL(NS_ConvertUTF16toUTF8(subjectStr), esc_Query,
                      subjectStrEscaped, mozilla::fallible);
    if (NS_FAILED(rv))
      return;

    aPath.Append(subjectStrEscaped);
  }
}

nsresult
FSURLEncoded::GetEncodedSubmission(nsIURI* aURI,
                                   nsIInputStream** aPostDataStream)
{
  nsresult rv = NS_OK;

  *aPostDataStream = nullptr;

  if (mMethod == NS_FORM_METHOD_POST) {

    bool isMailto = false;
    aURI->SchemeIs("mailto", &isMailto);
    if (isMailto) {

      nsAutoCString path;
      rv = aURI->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);

      HandleMailtoSubject(path);

      // Append the body to and force-plain-text args to the mailto line
      nsAutoCString escapedBody;
      if (NS_WARN_IF(!NS_Escape(mQueryString, escapedBody, url_XAlphas))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

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

      mimeStream->AddHeader("Content-Type",
                            "application/x-www-form-urlencoded");
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
      nsAutoCString path;
      rv = aURI->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);
      // Bug 42616: Trim off named anchor and save it to add later
      int32_t namedAnchorPos = path.FindChar('#');
      nsAutoCString namedAnchor;
      if (kNotFound != namedAnchorPos) {
        path.Right(namedAnchor, (path.Length() - namedAnchorPos));
        path.Truncate(namedAnchorPos);
      }

      // Chop off old query string (bug 25330, 57333)
      // Only do this for GET not POST (bug 41585)
      int32_t queryStart = path.FindChar('?');
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
FSURLEncoded::URLEncode(const nsAString& aStr, nsACString& aEncoded)
{
  // convert to CRLF breaks
  int32_t convertedBufLength = 0;
  char16_t* convertedBuf =
    nsLinebreakConverter::ConvertUnicharLineBreaks(aStr.BeginReading(),
                                                   nsLinebreakConverter::eLinebreakAny,
                                                   nsLinebreakConverter::eLinebreakNet,
                                                   aStr.Length(),
                                                   &convertedBufLength);
  NS_ENSURE_TRUE(convertedBuf, NS_ERROR_OUT_OF_MEMORY);

  nsAutoString convertedString;
  convertedString.Adopt(convertedBuf, convertedBufLength);

  nsAutoCString encodedBuf;
  nsresult rv = EncodeVal(convertedString, encodedBuf, false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_WARN_IF(!NS_Escape(encodedBuf, aEncoded, url_XPAlphas))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

} // anonymous namespace

// --------------------------------------------------------------------------

FSMultipartFormData::FSMultipartFormData(NotNull<const Encoding*> aEncoding,
                                         nsIContent* aOriginatingElement)
  : EncodingFormSubmission(aEncoding, aOriginatingElement)
{
  mPostData =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  mPostDataStream = do_QueryInterface(mPostData);
  mTotalLength = 0;

  mBoundary.AssignLiteral("---------------------------");
  mBoundary.AppendInt(rand());
  mBoundary.AppendInt(rand());
  mBoundary.AppendInt(rand());
}

FSMultipartFormData::~FSMultipartFormData()
{
  NS_ASSERTION(mPostDataChunk.IsEmpty(), "Left unsubmitted data");
}

nsIInputStream*
FSMultipartFormData::GetSubmissionBody(uint64_t* aContentLength)
{
  // Finish data
  mPostDataChunk += NS_LITERAL_CSTRING("--") + mBoundary
                  + NS_LITERAL_CSTRING("--" CRLF);

  // Add final data input stream
  AddPostDataStream();

  *aContentLength = mTotalLength;
  return mPostDataStream;
}

nsresult
FSMultipartFormData::AddNameValuePair(const nsAString& aName,
                                      const nsAString& aValue)
{
  nsCString valueStr;
  nsAutoCString encodedVal;
  nsresult rv = EncodeVal(aValue, encodedVal, false);
  NS_ENSURE_SUCCESS(rv, rv);

  valueStr.Adopt(nsLinebreakConverter::
                 ConvertLineBreaks(encodedVal.get(),
                                   nsLinebreakConverter::eLinebreakAny,
                                   nsLinebreakConverter::eLinebreakNet));

  nsAutoCString nameStr;
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
FSMultipartFormData::AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob)
{
  // Encode the control name
  nsAutoCString nameStr;
  nsresult rv = EncodeVal(aName, nameStr, true);
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult error;

  uint64_t size = 0;
  nsAutoCString filename;
  nsAutoCString contentType;
  nsCOMPtr<nsIInputStream> fileStream;

  if (aBlob) {
    nsAutoString filename16;

    RefPtr<File> file = aBlob->ToFile();
    if (file) {
      nsAutoString relativePath;
      file->GetRelativePath(relativePath);
      if (Directory::WebkitBlinkDirectoryPickerEnabled(nullptr, nullptr) &&
          !relativePath.IsEmpty()) {
        filename16 = relativePath;
      }

      if (filename16.IsEmpty()) {
        RetrieveFileName(aBlob, filename16);
      }
    }

    rv = EncodeVal(filename16, filename, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get content type
    nsAutoString contentType16;
    aBlob->GetType(contentType16);
    if (contentType16.IsEmpty()) {
      contentType16.AssignLiteral("application/octet-stream");
    }

    contentType.Adopt(nsLinebreakConverter::
                      ConvertLineBreaks(NS_ConvertUTF16toUTF8(contentType16).get(),
                                        nsLinebreakConverter::eLinebreakAny,
                                        nsLinebreakConverter::eLinebreakSpace));

    // Get input stream
    aBlob->GetInternalStream(getter_AddRefs(fileStream), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    // Get size
    size = aBlob->GetSize(error);
    if (error.Failed()) {
      error.SuppressException();
      fileStream = nullptr;
    }

    if (fileStream) {
      // Create buffered stream (for efficiency)
      nsCOMPtr<nsIInputStream> bufferedStream;
      rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                     fileStream, 8192);
      NS_ENSURE_SUCCESS(rv, rv);

      fileStream = bufferedStream;
    }
  } else {
    contentType.AssignLiteral("application/octet-stream");
  }

  AddDataChunk(nameStr, filename, contentType, fileStream, size);
  return NS_OK;
}

nsresult
FSMultipartFormData::AddNameDirectoryPair(const nsAString& aName,
                                          Directory* aDirectory)
{
  if (!Directory::WebkitBlinkDirectoryPickerEnabled(nullptr, nullptr)) {
    return NS_OK;
  }

  // Encode the control name
  nsAutoCString nameStr;
  nsresult rv = EncodeVal(aName, nameStr, true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString dirname;
  nsAutoString dirname16;

  ErrorResult error;
  nsAutoString path;
  aDirectory->GetPath(path, error);
  if (NS_WARN_IF(error.Failed())) {
    error.SuppressException();
  } else {
    dirname16 = path;
  }

  if (dirname16.IsEmpty()) {
    RetrieveDirectoryName(aDirectory, dirname16);
  }

  rv = EncodeVal(dirname16, dirname, true);
  NS_ENSURE_SUCCESS(rv, rv);

  AddDataChunk(nameStr, dirname,
               NS_LITERAL_CSTRING("application/octet-stream"),
               nullptr, 0);
  return NS_OK;
}

void
FSMultipartFormData::AddDataChunk(const nsACString& aName,
                                  const nsACString& aFilename,
                                  const nsACString& aContentType,
                                  nsIInputStream* aInputStream,
                                  uint64_t aInputStreamSize)
{
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
       + aName + NS_LITERAL_CSTRING("\"; filename=\"")
       + aFilename + NS_LITERAL_CSTRING("\"" CRLF)
       + NS_LITERAL_CSTRING("Content-Type: ")
       + aContentType + NS_LITERAL_CSTRING(CRLF CRLF);

  // We should not try to append an invalid stream. That will happen for example
  // if we try to update a file that actually do not exist.
  if (aInputStream) {
    // We need to dump the data up to this point into the POST data stream
    // here, since we're about to add the file input stream
    AddPostDataStream();

    mPostData->AppendStream(aInputStream);
    mTotalLength += aInputStreamSize;
  }

  // CRLF after file
  mPostDataChunk.AppendLiteral(CRLF);
}

nsresult
FSMultipartFormData::GetEncodedSubmission(nsIURI* aURI,
                                          nsIInputStream** aPostDataStream)
{
  nsresult rv;

  // Make header
  nsCOMPtr<nsIMIMEInputStream> mimeStream
    = do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString contentType;
  GetContentType(contentType);
  mimeStream->AddHeader("Content-Type", contentType.get());
  mimeStream->SetAddContentLength(true);
  uint64_t unused;
  mimeStream->SetData(GetSubmissionBody(&unused));

  mimeStream.forget(aPostDataStream);

  return NS_OK;
}

nsresult
FSMultipartFormData::AddPostDataStream()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIInputStream> postDataChunkStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(postDataChunkStream),
                                mPostDataChunk);
  NS_ASSERTION(postDataChunkStream, "Could not open a stream for POST!");
  if (postDataChunkStream) {
    mPostData->AppendStream(postDataChunkStream);
    mTotalLength += mPostDataChunk.Length();
  }

  mPostDataChunk.Truncate();

  return rv;
}

// --------------------------------------------------------------------------

namespace {

class FSTextPlain : public EncodingFormSubmission
{
public:
  FSTextPlain(NotNull<const Encoding*> aEncoding,
              nsIContent* aOriginatingElement)
    : EncodingFormSubmission(aEncoding, aOriginatingElement)
  {
  }

  virtual nsresult
  AddNameValuePair(const nsAString& aName, const nsAString& aValue) override;

  virtual nsresult
  AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob) override;

  virtual nsresult
  AddNameDirectoryPair(const nsAString& aName, Directory* aDirectory) override;

  virtual nsresult
  GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream) override;

private:
  nsString mBody;
};

nsresult
FSTextPlain::AddNameValuePair(const nsAString& aName, const nsAString& aValue)
{
  // XXX This won't work well with a name like "a=b" or "a\nb" but I suppose
  // text/plain doesn't care about that.  Parsers aren't built for escaped
  // values so we'll have to live with it.
  mBody.Append(aName + NS_LITERAL_STRING("=") + aValue +
               NS_LITERAL_STRING(CRLF));

  return NS_OK;
}

nsresult
FSTextPlain::AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob)
{
  nsAutoString filename;
  RetrieveFileName(aBlob, filename);
  AddNameValuePair(aName, filename);
  return NS_OK;
}

nsresult
FSTextPlain::AddNameDirectoryPair(const nsAString& aName,
                                  Directory* aDirectory)
{
  nsAutoString dirname;
  RetrieveDirectoryName(aDirectory, dirname);
  AddNameValuePair(aName, dirname);
  return NS_OK;
}

nsresult
FSTextPlain::GetEncodedSubmission(nsIURI* aURI,
                                  nsIInputStream** aPostDataStream)
{
  nsresult rv = NS_OK;

  // XXX HACK We are using the standard URL mechanism to give the body to the
  // mailer instead of passing the post data stream to it, since that sounds
  // hard.
  bool isMailto = false;
  aURI->SchemeIs("mailto", &isMailto);
  if (isMailto) {
    nsAutoCString path;
    rv = aURI->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    HandleMailtoSubject(path);

    // Append the body to and force-plain-text args to the mailto line
    nsAutoCString escapedBody;
    if (NS_WARN_IF(!NS_Escape(NS_ConvertUTF16toUTF8(mBody), escapedBody,
                              url_XAlphas))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

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

} // anonymous namespace

// --------------------------------------------------------------------------

EncodingFormSubmission::EncodingFormSubmission(
  NotNull<const Encoding*> aEncoding,
  nsIContent* aOriginatingElement)
  : HTMLFormSubmission(aEncoding, aOriginatingElement)
{
  if (!aEncoding->CanEncodeEverything()) {
    nsAutoCString name;
    aEncoding->Name(name);
    NS_ConvertUTF8toUTF16 nameUtf16(name);
    const char16_t* namePtr = nameUtf16.get();
    SendJSWarning(aOriginatingElement ? aOriginatingElement->GetOwnerDocument()
                                      : nullptr,
                  "CannotEncodeAllUnicode",
                  &namePtr,
                  1);
  }
}

EncodingFormSubmission::~EncodingFormSubmission()
{
}

// i18n helper routines
nsresult
EncodingFormSubmission::EncodeVal(const nsAString& aStr, nsCString& aOut,
                                  bool aHeaderEncode)
{
  nsresult rv;
  const Encoding* ignored;
  Tie(rv, ignored) = mEncoding->Encode(aStr, aOut);
  if (NS_FAILED(rv)) {
    return rv;
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

namespace {

NotNull<const Encoding*>
GetSubmitEncoding(nsGenericHTMLElement* aForm)
{
  nsAutoString acceptCharsetValue;
  aForm->GetAttr(kNameSpaceID_None, nsGkAtoms::acceptcharset,
                 acceptCharsetValue);

  int32_t charsetLen = acceptCharsetValue.Length();
  if (charsetLen > 0) {
    int32_t offset=0;
    int32_t spPos=0;
    // get charset from charsets one by one
    do {
      spPos = acceptCharsetValue.FindChar(char16_t(' '), offset);
      int32_t cnt = ((-1==spPos)?(charsetLen-offset):(spPos-offset));
      if (cnt > 0) {
        nsAutoString uCharset;
        acceptCharsetValue.Mid(uCharset, offset, cnt);

        auto encoding = Encoding::ForLabelNoReplacement(uCharset);
        if (encoding) {
          return WrapNotNull(encoding);
        }
      }
      offset = spPos + 1;
    } while (spPos != -1);
  }
  // if there are no accept-charset or all the charset are not supported
  // Get the charset from document
  nsIDocument* doc = aForm->GetComposedDoc();
  if (doc) {
    return Encoding::ForName(doc->GetDocumentCharacterSet());
  }
  return WrapNotNull(UTF_8_ENCODING);
}

void
GetEnumAttr(nsGenericHTMLElement* aContent,
            nsIAtom* atom, int32_t* aValue)
{
  const nsAttrValue* value = aContent->GetParsedAttr(atom);
  if (value && value->Type() == nsAttrValue::eEnum) {
    *aValue = value->GetEnumValue();
  }
}

} // anonymous namespace

/* static */ nsresult
HTMLFormSubmission::GetFromForm(nsGenericHTMLElement* aForm,
                                nsGenericHTMLElement* aOriginatingElement,
                                HTMLFormSubmission** aFormSubmission)
{
  // Get all the information necessary to encode the form data
  NS_ASSERTION(aForm->GetComposedDoc(),
               "Should have doc if we're building submission!");

  // Get encoding type (default: urlencoded)
  int32_t enctype = NS_FORM_ENCTYPE_URLENCODED;
  if (aOriginatingElement &&
      aOriginatingElement->HasAttr(kNameSpaceID_None, nsGkAtoms::formenctype)) {
    GetEnumAttr(aOriginatingElement, nsGkAtoms::formenctype, &enctype);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::enctype, &enctype);
  }

  // Get method (default: GET)
  int32_t method = NS_FORM_METHOD_GET;
  if (aOriginatingElement &&
      aOriginatingElement->HasAttr(kNameSpaceID_None, nsGkAtoms::formmethod)) {
    GetEnumAttr(aOriginatingElement, nsGkAtoms::formmethod, &method);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::method, &method);
  }

  // Get encoding
  auto encoding = GetSubmitEncoding(aForm)->OutputEncoding();

  // Choose encoder
  if (method == NS_FORM_METHOD_POST &&
      enctype == NS_FORM_ENCTYPE_MULTIPART) {
    *aFormSubmission = new FSMultipartFormData(encoding, aOriginatingElement);
  } else if (method == NS_FORM_METHOD_POST &&
             enctype == NS_FORM_ENCTYPE_TEXTPLAIN) {
    *aFormSubmission = new FSTextPlain(encoding, aOriginatingElement);
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
      const char16_t* enctypeStrPtr = enctypeStr.get();
      SendJSWarning(doc, "ForgotPostWarning",
                    &enctypeStrPtr, 1);
    }
    *aFormSubmission =
      new FSURLEncoded(encoding, method, doc, aOriginatingElement);
  }

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
