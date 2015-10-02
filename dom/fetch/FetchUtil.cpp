#include "FetchUtil.h"

#include "nsError.h"
#include "nsIUnicodeDecoder.h"
#include "nsString.h"

#include "mozilla/dom/EncodingUtils.h"

namespace mozilla {
namespace dom {

namespace {
class StreamDecoder final
{
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  nsString mDecoded;

public:
  StreamDecoder()
    : mDecoder(EncodingUtils::DecoderForEncoding("UTF-8"))
  {
    MOZ_ASSERT(mDecoder);
  }

  nsresult
  AppendText(const char* aSrcBuffer, uint32_t aSrcBufferLen)
  {
    int32_t destBufferLen;
    nsresult rv =
      mDecoder->GetMaxLength(aSrcBuffer, aSrcBufferLen, &destBufferLen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mDecoded.SetCapacity(mDecoded.Length() + destBufferLen, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    char16_t* destBuffer = mDecoded.BeginWriting() + mDecoded.Length();
    int32_t totalChars = mDecoded.Length();

    int32_t srcLen = (int32_t) aSrcBufferLen;
    int32_t outLen = destBufferLen;
    rv = mDecoder->Convert(aSrcBuffer, &srcLen, destBuffer, &outLen);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    totalChars += outLen;
    mDecoded.SetLength(totalChars);

    return NS_OK;
  }

  nsString&
  GetText()
  {
    return mDecoded;
  }
};
}

// static
nsresult
FetchUtil::GetValidRequestMethod(const nsACString& aMethod, nsCString& outMethod)
{
  nsAutoCString upperCaseMethod(aMethod);
  ToUpperCase(upperCaseMethod);
  if (upperCaseMethod.EqualsLiteral("CONNECT") ||
      upperCaseMethod.EqualsLiteral("TRACE") ||
      upperCaseMethod.EqualsLiteral("TRACK") ||
      !NS_IsValidHTTPToken(aMethod)) {
    outMethod.SetIsVoid(true);
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (upperCaseMethod.EqualsLiteral("DELETE") ||
      upperCaseMethod.EqualsLiteral("GET") ||
      upperCaseMethod.EqualsLiteral("HEAD") ||
      upperCaseMethod.EqualsLiteral("OPTIONS") ||
      upperCaseMethod.EqualsLiteral("POST") ||
      upperCaseMethod.EqualsLiteral("PUT")) {
    outMethod = upperCaseMethod;
  }
  else {
    outMethod = aMethod; // Case unchanged for non-standard methods
  }
  return NS_OK;
}

// static
void
FetchUtil::ConsumeArrayBuffer(JSContext* aCx,
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
FetchUtil::ConsumeBlob(nsISupports* aParent, const nsString& aMimeType,
                       uint32_t aInputLength, uint8_t* aInput,
                       ErrorResult& aRv)
{
  nsRefPtr<Blob> blob =
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
already_AddRefed<nsFormData>
FetchUtil::ConsumeFormData(nsIGlobalObject* aParent, const nsCString& aMimeType,
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
      aRv.ThrowTypeError(MSG_BAD_FORMDATA);
      return nullptr;
    }

    nsRefPtr<nsFormData> fd = parser.FormData();
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

    nsRefPtr<nsFormData> fd = new nsFormData(aParent);
    FillFormIterator iterator(fd);
    DebugOnly<bool> status = params.ForEach(iterator);
    MOZ_ASSERT(status);

    return fd.forget();
  }

  aRv.ThrowTypeError(MSG_BAD_FORMDATA);
  return nullptr;
}

// static
nsresult
FetchUtil::ConsumeText(uint32_t aInputLength, uint8_t* aInput,
                       nsString& aText)
{
  StreamDecoder decoder;
  nsresult rv = decoder.AppendText(reinterpret_cast<char*>(aInput),
                                   aInputLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  aText = decoder.GetText();
  return NS_OK;
}

// static
void
FetchUtil::ConsumeJson(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                       const nsString& aStr, ErrorResult& aRv)
{
  aRv.MightThrowJSException();

  AutoForceSetExceptionOnContext forceExn(aCx);
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
