#ifndef mozilla_dom_FetchUtil_h
#define mozilla_dom_FetchUtil_h

#include "nsString.h"
#include "nsError.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"

class nsIPrincipal;
class nsIDocument;
class nsIHttpChannel;

namespace mozilla {
namespace dom {

class InternalRequest;

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
   * Extracts an HTTP header from a substring range.
   */
  static bool
  ExtractHeader(nsACString::const_iterator& aStart,
                nsACString::const_iterator& aEnd,
                nsCString& aHeaderName,
                nsCString& aHeaderValue,
                bool* aWasEmptyHeader);

  static nsresult
  SetRequestReferrer(nsIPrincipal* aPrincipal,
                     nsIDocument* aDoc,
                     nsIHttpChannel* aChannel,
                     InternalRequest* aRequest);

};

} // namespace dom
} // namespace mozilla
#endif
