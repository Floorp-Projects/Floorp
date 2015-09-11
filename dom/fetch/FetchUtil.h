#ifndef mozilla_dom_FetchUtil_h
#define mozilla_dom_FetchUtil_h

#include "nsString.h"
#include "nsError.h"

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
};

} // namespace dom
} // namespace mozilla
#endif
