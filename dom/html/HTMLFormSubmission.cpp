/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFormSubmission.h"
#include "HTMLFormElement.h"
#include "HTMLFormSubmissionConstants.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsGkAtoms.h"
#include "nsIFormControl.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsDirectoryServiceDefs.h"
#include "nsStringStream.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsLinebreakConverter.h"
#include "nsEscape.h"
#include "nsUnicharUtils.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsIScriptError.h"
#include "nsCExternalHandlerService.h"
#include "nsContentUtils.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/RandomNum.h"

#include <tuple>

namespace mozilla::dom {

namespace {

void SendJSWarning(Document* aDocument, const char* aWarningName,
                   const nsTArray<nsString>& aWarningArgs) {
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "HTML"_ns,
                                  aDocument, nsContentUtils::eFORMS_PROPERTIES,
                                  aWarningName, aWarningArgs);
}

void RetrieveFileName(Blob* aBlob, nsAString& aFilename) {
  if (!aBlob) {
    return;
  }

  RefPtr<File> file = aBlob->ToFile();
  if (file) {
    file->GetName(aFilename);
  }
}

void RetrieveDirectoryName(Directory* aDirectory, nsAString& aDirname) {
  MOZ_ASSERT(aDirectory);

  ErrorResult rv;
  aDirectory->GetName(aDirname, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    aDirname.Truncate();
  }
}

// --------------------------------------------------------------------------

class FSURLEncoded : public EncodingFormSubmission {
 public:
  /**
   * @param aEncoding the character encoding of the form
   * @param aMethod the method of the submit (either NS_FORM_METHOD_GET or
   *        NS_FORM_METHOD_POST).
   */
  FSURLEncoded(nsIURI* aActionURL, const nsAString& aTarget,
               NotNull<const Encoding*> aEncoding, int32_t aMethod,
               Document* aDocument, Element* aSubmitter)
      : EncodingFormSubmission(aActionURL, aTarget, aEncoding, aSubmitter),
        mMethod(aMethod),
        mDocument(aDocument),
        mWarnedFileControl(false) {}

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override;

  virtual nsresult AddNameBlobPair(const nsAString& aName,
                                   Blob* aBlob) override;

  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) override;

  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) override;

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
  nsCOMPtr<Document> mDocument;

  /** Whether or not we have warned about a file control not being submitted */
  bool mWarnedFileControl;
};

nsresult FSURLEncoded::AddNameValuePair(const nsAString& aName,
                                        const nsAString& aValue) {
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
    mQueryString += convName + "="_ns + convValue;
  } else {
    mQueryString += "&"_ns + convName + "="_ns + convValue;
  }

  return NS_OK;
}

nsresult FSURLEncoded::AddNameBlobPair(const nsAString& aName, Blob* aBlob) {
  if (!mWarnedFileControl) {
    SendJSWarning(mDocument, "ForgotFileEnctypeWarning", nsTArray<nsString>());
    mWarnedFileControl = true;
  }

  nsAutoString filename;
  RetrieveFileName(aBlob, filename);
  return AddNameValuePair(aName, filename);
}

nsresult FSURLEncoded::AddNameDirectoryPair(const nsAString& aName,
                                            Directory* aDirectory) {
  // No warning about because Directory objects are never sent via form.

  nsAutoString dirname;
  RetrieveDirectoryName(aDirectory, dirname);
  return AddNameValuePair(aName, dirname);
}

void HandleMailtoSubject(nsCString& aPath) {
  // Walk through the string and see if we have a subject already.
  bool hasSubject = false;
  bool hasParams = false;
  int32_t paramSep = aPath.FindChar('?');
  while (paramSep != kNotFound && paramSep < (int32_t)aPath.Length()) {
    hasParams = true;

    // Get the end of the name at the = op.  If it is *after* the next &,
    // assume that someone made a parameter without an = in it
    int32_t nameEnd = aPath.FindChar('=', paramSep + 1);
    int32_t nextParamSep = aPath.FindChar('&', paramSep + 1);
    if (nextParamSep == kNotFound) {
      nextParamSep = aPath.Length();
    }

    // If the = op is after the &, this parameter is a name without value.
    // If there is no = op, same thing.
    if (nameEnd == kNotFound || nextParamSep < nameEnd) {
      nameEnd = nextParamSep;
    }

    if (nameEnd != kNotFound) {
      if (Substring(aPath, paramSep + 1, nameEnd - (paramSep + 1))
              .LowerCaseEqualsLiteral("subject")) {
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
    nsAutoString brandName;
    nsresult rv = nsContentUtils::GetLocalizedString(
        nsContentUtils::eBRAND_PROPERTIES, "brandShortName", brandName);
    if (NS_FAILED(rv)) return;
    nsAutoString subjectStr;
    rv = nsContentUtils::FormatLocalizedString(
        subjectStr, nsContentUtils::eFORMS_PROPERTIES, "DefaultFormSubject",
        brandName);
    if (NS_FAILED(rv)) return;
    aPath.AppendLiteral("subject=");
    nsCString subjectStrEscaped;
    rv = NS_EscapeURL(NS_ConvertUTF16toUTF8(subjectStr), esc_Query,
                      subjectStrEscaped, mozilla::fallible);
    if (NS_FAILED(rv)) return;

    aPath.Append(subjectStrEscaped);
  }
}

nsresult FSURLEncoded::GetEncodedSubmission(nsIURI* aURI,
                                            nsIInputStream** aPostDataStream,
                                            nsCOMPtr<nsIURI>& aOutURI) {
  nsresult rv = NS_OK;
  aOutURI = aURI;

  *aPostDataStream = nullptr;

  if (mMethod == NS_FORM_METHOD_POST) {
    if (aURI->SchemeIs("mailto")) {
      nsAutoCString path;
      rv = aURI->GetPathQueryRef(path);
      NS_ENSURE_SUCCESS(rv, rv);

      HandleMailtoSubject(path);

      // Append the body to and force-plain-text args to the mailto line
      nsAutoCString escapedBody;
      if (NS_WARN_IF(!NS_Escape(mQueryString, escapedBody, url_XAlphas))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      path += "&force-plain-text=Y&body="_ns + escapedBody;

      return NS_MutateURI(aURI).SetPathQueryRef(path).Finalize(aOutURI);
    } else {
      nsCOMPtr<nsIInputStream> dataStream;
      rv = NS_NewCStringInputStream(getter_AddRefs(dataStream),
                                    std::move(mQueryString));
      NS_ENSURE_SUCCESS(rv, rv);
      mQueryString.Truncate();

      nsCOMPtr<nsIMIMEInputStream> mimeStream(
          do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      mimeStream->AddHeader("Content-Type",
                            "application/x-www-form-urlencoded");
      mimeStream->SetData(dataStream);

      mimeStream.forget(aPostDataStream);
    }

  } else {
    // Get the full query string
    if (aURI->SchemeIs("javascript")) {
      return NS_OK;
    }

    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
    if (url) {
      // Make sure that we end up with a query component in the URL.  If
      // mQueryString is empty, nsIURI::SetQuery() will remove the query
      // component, which is not what we want.
      rv = NS_MutateURI(aURI)
               .SetQuery(mQueryString.IsEmpty() ? "?"_ns : mQueryString)
               .Finalize(aOutURI);
    } else {
      nsAutoCString path;
      rv = aURI->GetPathQueryRef(path);
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

      rv = NS_MutateURI(aURI).SetPathQueryRef(path).Finalize(aOutURI);
    }
  }

  return rv;
}

// i18n helper routines
nsresult FSURLEncoded::URLEncode(const nsAString& aStr, nsACString& aEncoded) {
  nsAutoCString encodedBuf;
  // We encode with eValueEncode because the urlencoded format needs the newline
  // normalizations but percent-escapes characters that eNameEncode doesn't,
  // so calling NS_Escape would still be needed.
  nsresult rv = EncodeVal(aStr, encodedBuf, EncodeType::eValueEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_WARN_IF(!NS_Escape(encodedBuf, aEncoded, url_XPAlphas))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

}  // anonymous namespace

// --------------------------------------------------------------------------

FSMultipartFormData::FSMultipartFormData(nsIURI* aActionURL,
                                         const nsAString& aTarget,
                                         NotNull<const Encoding*> aEncoding,
                                         Element* aSubmitter)
    : EncodingFormSubmission(aActionURL, aTarget, aEncoding, aSubmitter) {
  mPostData = do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");

  nsCOMPtr<nsIInputStream> inputStream = do_QueryInterface(mPostData);
  MOZ_ASSERT(SameCOMIdentity(mPostData, inputStream));
  mPostDataStream = inputStream;

  mTotalLength = 0;

  mBoundary.AssignLiteral("---------------------------");
  mBoundary.AppendInt(static_cast<uint32_t>(mozilla::RandomUint64OrDie()));
  mBoundary.AppendInt(static_cast<uint32_t>(mozilla::RandomUint64OrDie()));
  mBoundary.AppendInt(static_cast<uint32_t>(mozilla::RandomUint64OrDie()));
}

FSMultipartFormData::~FSMultipartFormData() {
  NS_ASSERTION(mPostDataChunk.IsEmpty(), "Left unsubmitted data");
}

nsIInputStream* FSMultipartFormData::GetSubmissionBody(
    uint64_t* aContentLength) {
  // Finish data
  mPostDataChunk += "--"_ns + mBoundary + nsLiteralCString("--" CRLF);

  // Add final data input stream
  AddPostDataStream();

  *aContentLength = mTotalLength;
  return mPostDataStream;
}

nsresult FSMultipartFormData::AddNameValuePair(const nsAString& aName,
                                               const nsAString& aValue) {
  nsAutoCString encodedVal;
  nsresult rv = EncodeVal(aValue, encodedVal, EncodeType::eValueEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString nameStr;
  rv = EncodeVal(aName, nameStr, EncodeType::eNameEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make MIME block for name/value pair

  mPostDataChunk += "--"_ns + mBoundary + nsLiteralCString(CRLF) +
                    "Content-Disposition: form-data; name=\""_ns + nameStr +
                    nsLiteralCString("\"" CRLF CRLF) + encodedVal +
                    nsLiteralCString(CRLF);

  return NS_OK;
}

nsresult FSMultipartFormData::AddNameBlobPair(const nsAString& aName,
                                              Blob* aBlob) {
  MOZ_ASSERT(aBlob);

  // Encode the control name
  nsAutoCString nameStr;
  nsresult rv = EncodeVal(aName, nameStr, EncodeType::eNameEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult error;

  uint64_t size = 0;
  nsAutoCString filename;
  nsAutoCString contentType;
  nsCOMPtr<nsIInputStream> fileStream;
  nsAutoString filename16;

  RefPtr<File> file = aBlob->ToFile();
  if (file) {
    nsAutoString relativePath;
    file->GetRelativePath(relativePath);
    if (StaticPrefs::dom_webkitBlink_dirPicker_enabled() &&
        !relativePath.IsEmpty()) {
      filename16 = relativePath;
    }

    if (filename16.IsEmpty()) {
      RetrieveFileName(aBlob, filename16);
    }
  }

  rv = EncodeVal(filename16, filename, EncodeType::eFilenameEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get content type
  nsAutoString contentType16;
  aBlob->GetType(contentType16);
  if (contentType16.IsEmpty()) {
    contentType16.AssignLiteral("application/octet-stream");
  }

  NS_ConvertUTF16toUTF8 contentType8(contentType16);
  int32_t convertedBufLength = 0;
  char* convertedBuf = nsLinebreakConverter::ConvertLineBreaks(
      contentType8.get(), nsLinebreakConverter::eLinebreakAny,
      nsLinebreakConverter::eLinebreakSpace, contentType8.Length(),
      &convertedBufLength);
  contentType.Adopt(convertedBuf, convertedBufLength);

  // Get input stream
  aBlob->CreateInputStream(getter_AddRefs(fileStream), error);
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
                                   fileStream.forget(), 8192);
    NS_ENSURE_SUCCESS(rv, rv);

    fileStream = bufferedStream;
  }

  AddDataChunk(nameStr, filename, contentType, fileStream, size);
  return NS_OK;
}

nsresult FSMultipartFormData::AddNameDirectoryPair(const nsAString& aName,
                                                   Directory* aDirectory) {
  if (!StaticPrefs::dom_webkitBlink_dirPicker_enabled()) {
    return NS_OK;
  }

  // Encode the control name
  nsAutoCString nameStr;
  nsresult rv = EncodeVal(aName, nameStr, EncodeType::eNameEncode);
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

  rv = EncodeVal(dirname16, dirname, EncodeType::eFilenameEncode);
  NS_ENSURE_SUCCESS(rv, rv);

  AddDataChunk(nameStr, dirname, "application/octet-stream"_ns, nullptr, 0);
  return NS_OK;
}

void FSMultipartFormData::AddDataChunk(const nsACString& aName,
                                       const nsACString& aFilename,
                                       const nsACString& aContentType,
                                       nsIInputStream* aInputStream,
                                       uint64_t aInputStreamSize) {
  //
  // Make MIME block for name/value pair
  //
  // more appropriate than always using binary?
  mPostDataChunk += "--"_ns + mBoundary + nsLiteralCString(CRLF);
  mPostDataChunk += "Content-Disposition: form-data; name=\""_ns + aName +
                    "\"; filename=\""_ns + aFilename +
                    nsLiteralCString("\"" CRLF) + "Content-Type: "_ns +
                    aContentType + nsLiteralCString(CRLF CRLF);

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

nsresult FSMultipartFormData::GetEncodedSubmission(
    nsIURI* aURI, nsIInputStream** aPostDataStream, nsCOMPtr<nsIURI>& aOutURI) {
  nsresult rv;
  aOutURI = aURI;

  // Make header
  nsCOMPtr<nsIMIMEInputStream> mimeStream =
      do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString contentType;
  GetContentType(contentType);
  mimeStream->AddHeader("Content-Type", contentType.get());

  uint64_t bodySize;
  mimeStream->SetData(GetSubmissionBody(&bodySize));

  mimeStream.forget(aPostDataStream);

  return NS_OK;
}

nsresult FSMultipartFormData::AddPostDataStream() {
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

class FSTextPlain : public EncodingFormSubmission {
 public:
  FSTextPlain(nsIURI* aActionURL, const nsAString& aTarget,
              NotNull<const Encoding*> aEncoding, Element* aSubmitter)
      : EncodingFormSubmission(aActionURL, aTarget, aEncoding, aSubmitter) {}

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override;

  virtual nsresult AddNameBlobPair(const nsAString& aName,
                                   Blob* aBlob) override;

  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) override;

  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) override;

 private:
  nsString mBody;
};

nsresult FSTextPlain::AddNameValuePair(const nsAString& aName,
                                       const nsAString& aValue) {
  // XXX This won't work well with a name like "a=b" or "a\nb" but I suppose
  // text/plain doesn't care about that.  Parsers aren't built for escaped
  // values so we'll have to live with it.
  mBody.Append(aName + u"="_ns + aValue + NS_LITERAL_STRING_FROM_CSTRING(CRLF));

  return NS_OK;
}

nsresult FSTextPlain::AddNameBlobPair(const nsAString& aName, Blob* aBlob) {
  nsAutoString filename;
  RetrieveFileName(aBlob, filename);
  AddNameValuePair(aName, filename);
  return NS_OK;
}

nsresult FSTextPlain::AddNameDirectoryPair(const nsAString& aName,
                                           Directory* aDirectory) {
  nsAutoString dirname;
  RetrieveDirectoryName(aDirectory, dirname);
  AddNameValuePair(aName, dirname);
  return NS_OK;
}

nsresult FSTextPlain::GetEncodedSubmission(nsIURI* aURI,
                                           nsIInputStream** aPostDataStream,
                                           nsCOMPtr<nsIURI>& aOutURI) {
  nsresult rv = NS_OK;
  aOutURI = aURI;

  *aPostDataStream = nullptr;

  // XXX HACK We are using the standard URL mechanism to give the body to the
  // mailer instead of passing the post data stream to it, since that sounds
  // hard.
  if (aURI->SchemeIs("mailto")) {
    nsAutoCString path;
    rv = aURI->GetPathQueryRef(path);
    NS_ENSURE_SUCCESS(rv, rv);

    HandleMailtoSubject(path);

    // Append the body to and force-plain-text args to the mailto line
    nsAutoCString escapedBody;
    if (NS_WARN_IF(!NS_Escape(NS_ConvertUTF16toUTF8(mBody), escapedBody,
                              url_XAlphas))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    path += "&force-plain-text=Y&body="_ns + escapedBody;

    rv = NS_MutateURI(aURI).SetPathQueryRef(path).Finalize(aOutURI);
  } else {
    // Create data stream.
    // We use eValueEncode to send the data through the charset encoder and to
    // normalize linebreaks to use the "standard net" format (\r\n), but not
    // perform any other escaping. This means that names and values which
    // contain '=' or newlines are potentially ambiguously encoded, but that is
    // how text/plain is specced.
    nsCString cbody;
    EncodeVal(mBody, cbody, EncodeType::eValueEncode);

    nsCOMPtr<nsIInputStream> bodyStream;
    rv = NS_NewCStringInputStream(getter_AddRefs(bodyStream), std::move(cbody));
    if (!bodyStream) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Create mime stream with headers and such
    nsCOMPtr<nsIMIMEInputStream> mimeStream =
        do_CreateInstance("@mozilla.org/network/mime-input-stream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mimeStream->AddHeader("Content-Type", "text/plain");
    mimeStream->SetData(bodyStream);
    mimeStream.forget(aPostDataStream);
  }

  return rv;
}

}  // anonymous namespace

// --------------------------------------------------------------------------

HTMLFormSubmission::HTMLFormSubmission(
    nsIURI* aActionURL, const nsAString& aTarget,
    mozilla::NotNull<const mozilla::Encoding*> aEncoding)
    : mActionURL(aActionURL),
      mTarget(aTarget),
      mEncoding(aEncoding),
      mInitiatedFromUserInput(UserActivation::IsHandlingUserInput()) {
  MOZ_COUNT_CTOR(HTMLFormSubmission);
}

EncodingFormSubmission::EncodingFormSubmission(
    nsIURI* aActionURL, const nsAString& aTarget,
    NotNull<const Encoding*> aEncoding, Element* aSubmitter)
    : HTMLFormSubmission(aActionURL, aTarget, aEncoding) {
  if (!aEncoding->CanEncodeEverything()) {
    nsAutoCString name;
    aEncoding->Name(name);
    AutoTArray<nsString, 1> args;
    CopyUTF8toUTF16(name, *args.AppendElement());
    SendJSWarning(aSubmitter ? aSubmitter->GetOwnerDocument() : nullptr,
                  "CannotEncodeAllUnicode", args);
  }
}

EncodingFormSubmission::~EncodingFormSubmission() = default;

// i18n helper routines
nsresult EncodingFormSubmission::EncodeVal(const nsAString& aStr,
                                           nsCString& aOut,
                                           EncodeType aEncodeType) {
  nsresult rv;
  std::tie(rv, std::ignore) = mEncoding->Encode(aStr, aOut);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aEncodeType != EncodeType::eFilenameEncode) {
    // Normalize newlines
    int32_t convertedBufLength = 0;
    char* convertedBuf = nsLinebreakConverter::ConvertLineBreaks(
        aOut.get(), nsLinebreakConverter::eLinebreakAny,
        nsLinebreakConverter::eLinebreakNet, (int32_t)aOut.Length(),
        &convertedBufLength);
    aOut.Adopt(convertedBuf, convertedBufLength);
  }

  if (aEncodeType != EncodeType::eValueEncode) {
    // Percent-escape LF, CR and double quotes.
    int32_t offset = 0;
    while ((offset = aOut.FindCharInSet("\n\r\"", offset)) != kNotFound) {
      if (aOut[offset] == '\n') {
        aOut.ReplaceLiteral(offset, 1, "%0A");
      } else if (aOut[offset] == '\r') {
        aOut.ReplaceLiteral(offset, 1, "%0D");
      } else if (aOut[offset] == '"') {
        aOut.ReplaceLiteral(offset, 1, "%22");
      } else {
        MOZ_ASSERT(false);
        offset++;
        continue;
      }
    }
  }

  return NS_OK;
}

// --------------------------------------------------------------------------

namespace {

void GetEnumAttr(nsGenericHTMLElement* aContent, nsAtom* atom,
                 int32_t* aValue) {
  const nsAttrValue* value = aContent->GetParsedAttr(atom);
  if (value && value->Type() == nsAttrValue::eEnum) {
    *aValue = value->GetEnumValue();
  }
}

}  // anonymous namespace

/* static */
nsresult HTMLFormSubmission::GetFromForm(HTMLFormElement* aForm,
                                         nsGenericHTMLElement* aSubmitter,
                                         NotNull<const Encoding*>& aEncoding,
                                         HTMLFormSubmission** aFormSubmission) {
  // Get all the information necessary to encode the form data
  NS_ASSERTION(aForm->GetComposedDoc(),
               "Should have doc if we're building submission!");

  nsresult rv;

  // Get action
  nsCOMPtr<nsIURI> actionURL;
  rv = aForm->GetActionURL(getter_AddRefs(actionURL), aSubmitter);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if CSP allows this form-action
  nsCOMPtr<nsIContentSecurityPolicy> csp = aForm->GetCsp();
  if (csp) {
    bool permitsFormAction = true;

    // form-action is only enforced if explicitly defined in the
    // policy - do *not* consult default-src, see:
    // http://www.w3.org/TR/CSP2/#directive-default-src
    rv = csp->Permits(aForm, nullptr /* nsICSPEventListener */, actionURL,
                      nsIContentSecurityPolicy::FORM_ACTION_DIRECTIVE,
                      true /* aSpecific */, true /* aSendViolationReports */,
                      &permitsFormAction);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!permitsFormAction) {
      return NS_ERROR_CSP_FORM_ACTION_VIOLATION;
    }
  }

  // Get target
  // The target is the submitter element formtarget attribute if the element
  // is a submit control and has such an attribute.
  // Otherwise, the target is the form owner's target attribute,
  // if it has such an attribute.
  // Finally, if one of the child nodes of the head element is a base element
  // with a target attribute, then the value of the target attribute of the
  // first such base element; or, if there is no such element, the empty string.
  nsAutoString target;
  if (!(aSubmitter && aSubmitter->GetAttr(kNameSpaceID_None,
                                          nsGkAtoms::formtarget, target)) &&
      !aForm->GetAttr(kNameSpaceID_None, nsGkAtoms::target, target)) {
    aForm->GetBaseTarget(target);
  }

  // Get encoding type (default: urlencoded)
  int32_t enctype = NS_FORM_ENCTYPE_URLENCODED;
  if (aSubmitter &&
      aSubmitter->HasAttr(kNameSpaceID_None, nsGkAtoms::formenctype)) {
    GetEnumAttr(aSubmitter, nsGkAtoms::formenctype, &enctype);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::enctype, &enctype);
  }

  // Get method (default: GET)
  int32_t method = NS_FORM_METHOD_GET;
  if (aSubmitter &&
      aSubmitter->HasAttr(kNameSpaceID_None, nsGkAtoms::formmethod)) {
    GetEnumAttr(aSubmitter, nsGkAtoms::formmethod, &method);
  } else {
    GetEnumAttr(aForm, nsGkAtoms::method, &method);
  }

  if (method == NS_FORM_METHOD_DIALOG) {
    HTMLDialogElement* dialog = aForm->FirstAncestorOfType<HTMLDialogElement>();

    // If there isn't one, or if it does not have an open attribute, do
    // nothing.
    if (!dialog || !dialog->Open()) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString result;
    if (aSubmitter) {
      aSubmitter->ResultForDialogSubmit(result);
    }
    *aFormSubmission =
        new DialogFormSubmission(result, actionURL, target, aEncoding, dialog);
    return NS_OK;
  }

  MOZ_ASSERT(method != NS_FORM_METHOD_DIALOG);

  // Choose encoder
  if (method == NS_FORM_METHOD_POST && enctype == NS_FORM_ENCTYPE_MULTIPART) {
    *aFormSubmission =
        new FSMultipartFormData(actionURL, target, aEncoding, aSubmitter);
  } else if (method == NS_FORM_METHOD_POST &&
             enctype == NS_FORM_ENCTYPE_TEXTPLAIN) {
    *aFormSubmission =
        new FSTextPlain(actionURL, target, aEncoding, aSubmitter);
  } else {
    Document* doc = aForm->OwnerDoc();
    if (enctype == NS_FORM_ENCTYPE_MULTIPART ||
        enctype == NS_FORM_ENCTYPE_TEXTPLAIN) {
      AutoTArray<nsString, 1> args;
      nsString& enctypeStr = *args.AppendElement();
      if (aSubmitter &&
          aSubmitter->HasAttr(kNameSpaceID_None, nsGkAtoms::formenctype)) {
        aSubmitter->GetAttr(kNameSpaceID_None, nsGkAtoms::formenctype,
                            enctypeStr);
      } else {
        aForm->GetAttr(kNameSpaceID_None, nsGkAtoms::enctype, enctypeStr);
      }

      SendJSWarning(doc, "ForgotPostWarning", args);
    }
    *aFormSubmission =
        new FSURLEncoded(actionURL, target, aEncoding, method, doc, aSubmitter);
  }

  return NS_OK;
}

}  // namespace mozilla::dom
