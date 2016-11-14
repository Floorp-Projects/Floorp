#include "FetchUtil.h"

#include "nsError.h"
#include "nsIUnicodeDecoder.h"
#include "nsString.h"
#include "nsIDocument.h"

#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/InternalRequest.h"

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

// static
nsresult
FetchUtil::SetRequestReferrer(nsIPrincipal* aPrincipal,
                              nsIDocument* aDoc,
                              nsIHttpChannel* aChannel,
                              InternalRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString referrer;
  aRequest->GetReferrer(referrer);
  net::ReferrerPolicy policy = aRequest->GetReferrerPolicy();

  nsresult rv = NS_OK;
  if (referrer.IsEmpty()) {
    // This is the case request’s referrer is "no-referrer"
    rv = aChannel->SetReferrerWithPolicy(nullptr, net::RP_No_Referrer);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (referrer.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
    rv = nsContentUtils::SetFetchReferrerURIWithPolicy(aPrincipal,
                                                       aDoc,
                                                       aChannel,
                                                       policy);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // From "Determine request's Referrer" step 3
    // "If request's referrer is a URL, let referrerSource be request's
    // referrer."
    nsCOMPtr<nsIURI> referrerURI;
    rv = NS_NewURI(getter_AddRefs(referrerURI), referrer, nullptr, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aChannel->SetReferrerWithPolicy(referrerURI, policy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> referrerURI;
  aChannel->GetReferrer(getter_AddRefs(referrerURI));

  // Step 8 https://fetch.spec.whatwg.org/#main-fetch
  // If request’s referrer is not "no-referrer", set request’s referrer to
  // the result of invoking determine request’s referrer.
  if (referrerURI) {
    nsAutoCString spec;
    rv = referrerURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    aRequest->SetReferrer(NS_ConvertUTF8toUTF16(spec));
  } else {
    aRequest->SetReferrer(EmptyString());
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
