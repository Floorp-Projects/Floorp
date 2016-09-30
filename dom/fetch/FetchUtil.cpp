#include "FetchUtil.h"

#include "nsError.h"
#include "nsIUnicodeDecoder.h"
#include "nsString.h"

#include "mozilla/dom/EncodingUtils.h"

namespace mozilla {
namespace dom {

// static
nsresult
FetchUtil::GetValidRequestMethod(const nsACString& aMethod, nsCString& outMethod)
{
  nsAutoCString upperCaseMethod(aMethod);
  ToUpperCase(upperCaseMethod);
  if (!NS_IsValidHTTPToken(aMethod)) {
    outMethod.SetIsVoid(true);
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (upperCaseMethod.EqualsLiteral("CONNECT") ||
      upperCaseMethod.EqualsLiteral("TRACE") ||
      upperCaseMethod.EqualsLiteral("TRACK")) {
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

static bool
FindCRLF(nsACString::const_iterator& aStart,
         nsACString::const_iterator& aEnd)
{
  nsACString::const_iterator end(aEnd);
  return FindInReadable(NS_LITERAL_CSTRING("\r\n"), aStart, end);
}

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

// static
bool
FetchUtil::ExtractHeader(nsACString::const_iterator& aStart,
                         nsACString::const_iterator& aEnd,
                         nsCString& aHeaderName,
                         nsCString& aHeaderValue,
                         bool* aWasEmptyHeader)
{
  MOZ_ASSERT(aWasEmptyHeader);
  // Set it to a valid value here so we don't forget later.
  *aWasEmptyHeader = false;

  const char* beginning = aStart.get();
  nsACString::const_iterator end(aEnd);
  if (!FindCRLF(aStart, end)) {
    return false;
  }

  if (aStart.get() == beginning) {
    *aWasEmptyHeader = true;
    return true;
  }

  nsAutoCString header(beginning, aStart.get() - beginning);

  nsACString::const_iterator headerStart, iter, headerEnd;
  header.BeginReading(headerStart);
  header.EndReading(headerEnd);
  iter = headerStart;
  if (!FindCharInReadable(':', iter, headerEnd)) {
    return false;
  }

  aHeaderName.Assign(StringHead(header, iter - headerStart));
  aHeaderName.CompressWhitespace();
  if (!NS_IsValidHTTPToken(aHeaderName)) {
    return false;
  }

  aHeaderValue.Assign(Substring(++iter, headerEnd));
  if (!NS_IsReasonableHTTPHeaderValue(aHeaderValue)) {
    return false;
  }
  aHeaderValue.CompressWhitespace();

  return PushOverLine(aStart, aEnd);
}

} // namespace dom
} // namespace mozilla
