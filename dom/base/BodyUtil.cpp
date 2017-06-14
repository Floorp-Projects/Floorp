/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BodyUtil.h"

#include "nsError.h"
#include "nsString.h"
#include "nsIGlobalObject.h"
#include "mozilla/Encoding.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URLSearchParams.h"

namespace mozilla {
namespace dom {

namespace {

// Reads over a CRLF and positions start after it.
static bool
PushOverLine(nsACString::const_iterator& aStart,
	     const nsACString::const_iterator& aEnd)
{
  if (*aStart == nsCRT::CR && (aEnd - aStart > 1) && *(++aStart) == nsCRT::LF) {
    ++aStart; // advance to after CRLF
    return true;
  }

  return false;
}

class MOZ_STACK_CLASS FillFormIterator final
  : public URLSearchParams::ForEachIterator
{
public:
  explicit FillFormIterator(FormData* aFormData)
    : mFormData(aFormData)
  {
    MOZ_ASSERT(aFormData);
  }

  bool URLParamsIterator(const nsString& aName,
                         const nsString& aValue) override
  {
    ErrorResult rv;
    mFormData->Append(aName, aValue, rv);
    MOZ_ASSERT(!rv.Failed());
    return true;
  }

private:
  FormData* mFormData;
};

/**
 * A simple multipart/form-data parser as defined in RFC 2388 and RFC 2046.
 * This does not respect any encoding specified per entry, using UTF-8
 * throughout. This is as the Fetch spec states in the consume body algorithm.
 * Borrows some things from Necko's nsMultiMixedConv, but is simpler since
 * unlike Necko we do not have to deal with receiving incomplete chunks of data.
 *
 * This parser will fail the entire parse on any invalid entry, so it will
 * never return a partially filled FormData.
 * The content-disposition header is used to figure out the name and filename
 * entries. The inclusion of the filename parameter decides if the entry is
 * inserted into the FormData as a string or a File.
 *
 * File blobs are copies of the underlying data string since we cannot adopt
 * char* chunks embedded within the larger body without significant effort.
 * FIXME(nsm): Bug 1127552 - We should add telemetry to calls to formData() and
 * friends to figure out if Fetch ends up copying big blobs to see if this is
 * worth optimizing.
 */
class MOZ_STACK_CLASS FormDataParser
{
private:
  RefPtr<FormData> mFormData;
  nsCString mMimeType;
  nsCString mData;

  // Entry state, reset in START_PART.
  nsCString mName;
  nsCString mFilename;
  nsCString mContentType;

  enum
  {
    START_PART,
    PARSE_HEADER,
    PARSE_BODY,
  } mState;

  nsIGlobalObject* mParentObject;

  // Reads over a boundary and sets start to the position after the end of the
  // boundary. Returns false if no boundary is found immediately.
  bool
  PushOverBoundary(const nsACString& aBoundaryString,
                   nsACString::const_iterator& aStart,
                   nsACString::const_iterator& aEnd)
  {
    // We copy the end iterator to keep the original pointing to the real end
    // of the string.
    nsACString::const_iterator end(aEnd);
    const char* beginning = aStart.get();
    if (FindInReadable(aBoundaryString, aStart, end)) {
      // We either should find the body immediately, or after 2 chars with the
      // 2 chars being '-', everything else is failure.
      if ((aStart.get() - beginning) == 0) {
        aStart.advance(aBoundaryString.Length());
        return true;
      }

      if ((aStart.get() - beginning) == 2) {
        if (*(--aStart) == '-' && *(--aStart) == '-') {
          aStart.advance(aBoundaryString.Length() + 2);
          return true;
        }
      }
    }

    return false;
  }

  bool
  ParseHeader(nsACString::const_iterator& aStart,
              nsACString::const_iterator& aEnd,
              bool* aWasEmptyHeader)
  {
    nsAutoCString headerName, headerValue;
    if (!FetchUtil::ExtractHeader(aStart, aEnd,
                                  headerName, headerValue,
                                  aWasEmptyHeader)) {
      return false;
    }
    if (*aWasEmptyHeader) {
      return true;
    }

    if (headerName.LowerCaseEqualsLiteral("content-disposition")) {
      nsCCharSeparatedTokenizer tokenizer(headerValue, ';');
      bool seenFormData = false;
      while (tokenizer.hasMoreTokens()) {
        const nsDependentCSubstring& token = tokenizer.nextToken();
        if (token.IsEmpty()) {
          continue;
        }

        if (token.EqualsLiteral("form-data")) {
          seenFormData = true;
          continue;
        }

        if (seenFormData &&
            StringBeginsWith(token, NS_LITERAL_CSTRING("name="))) {
          mName = StringTail(token, token.Length() - 5);
          mName.Trim(" \"");
          continue;
        }

        if (seenFormData &&
            StringBeginsWith(token, NS_LITERAL_CSTRING("filename="))) {
          mFilename = StringTail(token, token.Length() - 9);
          mFilename.Trim(" \"");
          continue;
        }
      }

      if (mName.IsVoid()) {
        // Could not parse a valid entry name.
        return false;
      }
    } else if (headerName.LowerCaseEqualsLiteral("content-type")) {
      mContentType = headerValue;
    }

    return true;
  }

  // The end of a body is marked by a CRLF followed by the boundary. So the
  // CRLF is part of the boundary and not the body, but any prior CRLFs are
  // part of the body. This will position the iterator at the beginning of the
  // boundary (after the CRLF).
  bool
  ParseBody(const nsACString& aBoundaryString,
            nsACString::const_iterator& aStart,
            nsACString::const_iterator& aEnd)
  {
    const char* beginning = aStart.get();

    // Find the boundary marking the end of the body.
    nsACString::const_iterator end(aEnd);
    if (!FindInReadable(aBoundaryString, aStart, end)) {
      return false;
    }

    // We found a boundary, strip the just prior CRLF, and consider
    // everything else the body section.
    if (aStart.get() - beginning < 2) {
      // Only the first entry can have a boundary right at the beginning. Even
      // an empty body will have a CRLF before the boundary. So this is
      // a failure.
      return false;
    }

    // Check that there is a CRLF right before the boundary.
    aStart.advance(-2);

    // Skip optional hyphens.
    if (*aStart == '-' && *(aStart.get()+1) == '-') {
      if (aStart.get() - beginning < 2) {
        return false;
      }

      aStart.advance(-2);
    }

    if (*aStart != nsCRT::CR || *(aStart.get()+1) != nsCRT::LF) {
      return false;
    }

    nsAutoCString body(beginning, aStart.get() - beginning);

    // Restore iterator to after the \r\n as we promised.
    // We do not need to handle the extra hyphens case since our boundary
    // parser in PushOverBoundary()
    aStart.advance(2);

    if (!mFormData) {
      mFormData = new FormData();
    }

    NS_ConvertUTF8toUTF16 name(mName);

    if (mFilename.IsVoid()) {
      ErrorResult rv;
      mFormData->Append(name, NS_ConvertUTF8toUTF16(body), rv);
      MOZ_ASSERT(!rv.Failed());
    } else {
      // Unfortunately we've to copy the data first since all our strings are
      // going to free it. We also need fallible alloc, so we can't just use
      // ToNewCString().
      char* copy = static_cast<char*>(moz_xmalloc(body.Length()));
      if (!copy) {
        NS_WARNING("Failed to copy File entry body.");
        return false;
      }
      nsCString::const_iterator bodyIter, bodyEnd;
      body.BeginReading(bodyIter);
      body.EndReading(bodyEnd);
      char *p = copy;
      while (bodyIter != bodyEnd) {
        *p++ = *bodyIter++;
      }
      p = nullptr;

      RefPtr<Blob> file =
        File::CreateMemoryFile(mParentObject,
                               reinterpret_cast<void *>(copy), body.Length(),
                               NS_ConvertUTF8toUTF16(mFilename),
                               NS_ConvertUTF8toUTF16(mContentType), /* aLastModifiedDate */ 0);
      Optional<nsAString> dummy;
      ErrorResult rv;
      mFormData->Append(name, *file, dummy, rv);
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
        return false;
      }
    }

    return true;
  }

public:
  FormDataParser(const nsACString& aMimeType, const nsACString& aData, nsIGlobalObject* aParent)
    : mMimeType(aMimeType), mData(aData), mState(START_PART), mParentObject(aParent)
  {
  }

  bool
  Parse()
  {
    // Determine boundary from mimetype.
    const char* boundaryId = nullptr;
    boundaryId = strstr(mMimeType.BeginWriting(), "boundary");
    if (!boundaryId) {
      return false;
    }

    boundaryId = strchr(boundaryId, '=');
    if (!boundaryId) {
      return false;
    }

    // Skip over '='.
    boundaryId++;

    char *attrib = (char *) strchr(boundaryId, ';');
    if (attrib) *attrib = '\0';

    nsAutoCString boundaryString(boundaryId);
    if (attrib) *attrib = ';';

    boundaryString.Trim(" \"");

    if (boundaryString.Length() == 0) {
      return false;
    }

    nsACString::const_iterator start, end;
    mData.BeginReading(start);
    // This should ALWAYS point to the end of data.
    // Helpers make copies.
    mData.EndReading(end);

    while (start != end) {
      switch(mState) {
        case START_PART:
          mName.SetIsVoid(true);
          mFilename.SetIsVoid(true);
          mContentType = NS_LITERAL_CSTRING("text/plain");

          // MUST start with boundary.
          if (!PushOverBoundary(boundaryString, start, end)) {
            return false;
          }

          if (start != end && *start == '-') {
            // End of data.
            if (!mFormData) {
              mFormData = new FormData();
            }
            return true;
          }

          if (!PushOverLine(start, end)) {
            return false;
          }
          mState = PARSE_HEADER;
          break;

        case PARSE_HEADER:
          bool emptyHeader;
          if (!ParseHeader(start, end, &emptyHeader)) {
            return false;
          }

          if (emptyHeader && !PushOverLine(start, end)) {
            return false;
          }

          mState = emptyHeader ? PARSE_BODY : PARSE_HEADER;
          break;

        case PARSE_BODY:
          if (mName.IsVoid()) {
            NS_WARNING("No content-disposition header with a valid name was "
                       "found. Failing at body parse.");
            return false;
          }

          if (!ParseBody(boundaryString, start, end)) {
            return false;
          }

          mState = START_PART;
          break;

        default:
          MOZ_CRASH("Invalid case");
      }
    }

    NS_NOTREACHED("Should never reach here.");
    return false;
  }

  already_AddRefed<FormData> GetFormData()
  {
    return mFormData.forget();
  }
};
}

// static
void
BodyUtil::ConsumeArrayBuffer(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aValue,
                              uint32_t aInputLength, uint8_t* aInput,
                              ErrorResult& aRv)
{
  JS::Rooted<JSObject*> arrayBuffer(aCx);
  arrayBuffer = JS_NewArrayBufferWithContents(aCx, aInputLength,
    reinterpret_cast<void *>(aInput));
  if (!arrayBuffer) {
    JS_ClearPendingException(aCx);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aValue.set(arrayBuffer);
}

// static
already_AddRefed<Blob>
BodyUtil::ConsumeBlob(nsISupports* aParent, const nsString& aMimeType,
                       uint32_t aInputLength, uint8_t* aInput,
                       ErrorResult& aRv)
{
  RefPtr<Blob> blob =
    Blob::CreateMemoryBlob(aParent,
                           reinterpret_cast<void *>(aInput), aInputLength,
                           aMimeType);

  if (!blob) {
    aRv.Throw(NS_ERROR_DOM_UNKNOWN_ERR);
    return nullptr;
  }
  return blob.forget();
}

// static
already_AddRefed<FormData>
BodyUtil::ConsumeFormData(nsIGlobalObject* aParent, const nsCString& aMimeType,
                           const nsCString& aStr, ErrorResult& aRv)
{
  NS_NAMED_LITERAL_CSTRING(formDataMimeType, "multipart/form-data");

  // Allow semicolon separated boundary/encoding suffix like multipart/form-data; boundary=
  // but disallow multipart/form-datafoobar.
  bool isValidFormDataMimeType = StringBeginsWith(aMimeType, formDataMimeType);

  if (isValidFormDataMimeType && aMimeType.Length() > formDataMimeType.Length()) {
    isValidFormDataMimeType = aMimeType[formDataMimeType.Length()] == ';';
  }

  if (isValidFormDataMimeType) {
    FormDataParser parser(aMimeType, aStr, aParent);
    if (!parser.Parse()) {
      aRv.ThrowTypeError<MSG_BAD_FORMDATA>();
      return nullptr;
    }

    RefPtr<FormData> fd = parser.GetFormData();
    MOZ_ASSERT(fd);
    return fd.forget();
  }

  NS_NAMED_LITERAL_CSTRING(urlDataMimeType, "application/x-www-form-urlencoded");
  bool isValidUrlEncodedMimeType = StringBeginsWith(aMimeType, urlDataMimeType);

  if (isValidUrlEncodedMimeType && aMimeType.Length() > urlDataMimeType.Length()) {
    isValidUrlEncodedMimeType = aMimeType[urlDataMimeType.Length()] == ';';
  }

  if (isValidUrlEncodedMimeType) {
    URLParams params;
    params.ParseInput(aStr);

    RefPtr<FormData> fd = new FormData(aParent);
    FillFormIterator iterator(fd);
    DebugOnly<bool> status = params.ForEach(iterator);
    MOZ_ASSERT(status);

    return fd.forget();
  }

  aRv.ThrowTypeError<MSG_BAD_FORMDATA>();
  return nullptr;
}

// static
nsresult
BodyUtil::ConsumeText(uint32_t aInputLength, uint8_t* aInput,
                       nsString& aText)
{
  nsresult rv =
    UTF_8_ENCODING->DecodeWithBOMRemoval(MakeSpan(aInput, aInputLength), aText);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

// static
void
BodyUtil::ConsumeJson(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                       const nsString& aStr, ErrorResult& aRv)
{
  aRv.MightThrowJSException();

  JS::Rooted<JS::Value> json(aCx);
  if (!JS_ParseJSON(aCx, aStr.get(), aStr.Length(), &json)) {
    if (!JS_IsExceptionPending(aCx)) {
      aRv.Throw(NS_ERROR_DOM_UNKNOWN_ERR);
      return;
    }

    JS::Rooted<JS::Value> exn(aCx);
    DebugOnly<bool> gotException = JS_GetPendingException(aCx, &exn);
    MOZ_ASSERT(gotException);

    JS_ClearPendingException(aCx);
    aRv.ThrowJSException(aCx, exn);
    return;
  }

  aValue.set(json);
}

} // namespace dom
} // namespace mozilla
