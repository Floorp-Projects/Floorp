#ifndef mozilla_dom_FetchUtil_h
#define mozilla_dom_FetchUtil_h

#include "nsString.h"
#include "nsError.h"
#include "nsFormData.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

class FetchUtil final
{
private:
  FetchUtil() = delete;

public:
  /**
  * Sets outMethod to a valid HTTP request method string based on an input method.
  * Implements checks and normalization as specified by the Fetch specification.
  * Returns NS_ERROR_DOM_SECURITY_ERR if the method is invalid.
  * Otherwise returns NS_OK and the normalized method via outMethod.
  */
  static nsresult
  GetValidRequestMethod(const nsACString& aMethod, nsCString& outMethod);

  /**
   * Creates an array buffer from an array, assigning the result to |aValue|.
   * The array buffer takes ownership of |aInput|, which must be allocated
   * by |malloc|.
   */
  static void
  ConsumeArrayBuffer(JSContext* aCx, JS::MutableHandle<JSObject*> aValue,
                     uint32_t aInputLength, uint8_t* aInput, ErrorResult& aRv);

  /**
   * Creates an in-memory blob from an array. The blob takes ownership of
   * |aInput|, which must be allocated by |malloc|.
   */
  static already_AddRefed<Blob>
  ConsumeBlob(nsISupports* aParent, const nsString& aMimeType,
              uint32_t aInputLength, uint8_t* aInput, ErrorResult& aRv);

  /**
   * Creates a form data object from a UTF-8 encoded |aStr|. Returns |nullptr|
   * and sets |aRv| to MSG_BAD_FORMDATA if |aStr| contains invalid data.
   */
  static already_AddRefed<nsFormData>
  ConsumeFormData(nsIGlobalObject* aParent, const nsCString& aMimeType,
                  const nsCString& aStr, ErrorResult& aRv);

  /**
   * UTF-8 decodes |aInput| into |aText|. The caller may free |aInput|
   * once this method returns.
   */
  static nsresult
  ConsumeText(uint32_t aInputLength, uint8_t* aInput, nsString& aText);

  /**
   * Parses a UTF-8 encoded |aStr| as JSON, assigning the result to |aValue|.
   * Sets |aRv| to a syntax error if |aStr| contains invalid data.
   */
  static void
  ConsumeJson(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
              const nsString& aStr, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla
#endif
