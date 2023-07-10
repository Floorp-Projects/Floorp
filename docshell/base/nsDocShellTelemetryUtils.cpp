/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellTelemetryUtils.h"

namespace {

using ErrorLabel = mozilla::Telemetry::LABELS_PAGE_LOAD_ERROR;

struct LoadErrorTelemetryResult {
  nsresult mValue;
  ErrorLabel mLabel;
};

static const LoadErrorTelemetryResult sResult[] = {
    {
        NS_ERROR_UNKNOWN_PROTOCOL,
        ErrorLabel::UNKNOWN_PROTOCOL,
    },
    {
        NS_ERROR_FILE_NOT_FOUND,
        ErrorLabel::FILE_NOT_FOUND,
    },
    {
        NS_ERROR_FILE_ACCESS_DENIED,
        ErrorLabel::FILE_ACCESS_DENIED,
    },
    {
        NS_ERROR_UNKNOWN_HOST,
        ErrorLabel::UNKNOWN_HOST,
    },
    {
        NS_ERROR_CONNECTION_REFUSED,
        ErrorLabel::CONNECTION_REFUSED,
    },
    {
        NS_ERROR_PROXY_BAD_GATEWAY,
        ErrorLabel::PROXY_BAD_GATEWAY,
    },
    {
        NS_ERROR_NET_INTERRUPT,
        ErrorLabel::NET_INTERRUPT,
    },
    {
        NS_ERROR_NET_TIMEOUT,
        ErrorLabel::NET_TIMEOUT,
    },
    {
        NS_ERROR_PROXY_GATEWAY_TIMEOUT,
        ErrorLabel::P_GATEWAY_TIMEOUT,
    },
    {
        NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION,
        ErrorLabel::CSP_FRAME_ANCEST,
    },
    {
        NS_ERROR_CSP_FORM_ACTION_VIOLATION,
        ErrorLabel::CSP_FORM_ACTION,
    },
    {
        NS_ERROR_XFO_VIOLATION,
        ErrorLabel::XFO_VIOLATION,
    },
    {
        NS_ERROR_PHISHING_URI,
        ErrorLabel::PHISHING_URI,
    },
    {
        NS_ERROR_MALWARE_URI,
        ErrorLabel::MALWARE_URI,
    },
    {
        NS_ERROR_UNWANTED_URI,
        ErrorLabel::UNWANTED_URI,
    },
    {
        NS_ERROR_HARMFUL_URI,
        ErrorLabel::HARMFUL_URI,
    },
    {
        NS_ERROR_CONTENT_CRASHED,
        ErrorLabel::CONTENT_CRASHED,
    },
    {
        NS_ERROR_FRAME_CRASHED,
        ErrorLabel::FRAME_CRASHED,
    },
    {
        NS_ERROR_BUILDID_MISMATCH,
        ErrorLabel::BUILDID_MISMATCH,
    },
    {
        NS_ERROR_NET_RESET,
        ErrorLabel::NET_RESET,
    },
    {
        NS_ERROR_MALFORMED_URI,
        ErrorLabel::MALFORMED_URI,
    },
    {
        NS_ERROR_REDIRECT_LOOP,
        ErrorLabel::REDIRECT_LOOP,
    },
    {
        NS_ERROR_UNKNOWN_SOCKET_TYPE,
        ErrorLabel::UNKNOWN_SOCKET,
    },
    {
        NS_ERROR_DOCUMENT_NOT_CACHED,
        ErrorLabel::DOCUMENT_N_CACHED,
    },
    {
        NS_ERROR_OFFLINE,
        ErrorLabel::OFFLINE,
    },
    {
        NS_ERROR_DOCUMENT_IS_PRINTMODE,
        ErrorLabel::DOC_PRINTMODE,
    },
    {
        NS_ERROR_PORT_ACCESS_NOT_ALLOWED,
        ErrorLabel::PORT_ACCESS,
    },
    {
        NS_ERROR_UNKNOWN_PROXY_HOST,
        ErrorLabel::UNKNOWN_PROXY_HOST,
    },
    {
        NS_ERROR_PROXY_CONNECTION_REFUSED,
        ErrorLabel::PROXY_CONNECTION,
    },
    {
        NS_ERROR_PROXY_FORBIDDEN,
        ErrorLabel::PROXY_FORBIDDEN,
    },
    {
        NS_ERROR_PROXY_NOT_IMPLEMENTED,
        ErrorLabel::P_NOT_IMPLEMENTED,
    },
    {
        NS_ERROR_PROXY_AUTHENTICATION_FAILED,
        ErrorLabel::PROXY_AUTH,
    },
    {
        NS_ERROR_PROXY_TOO_MANY_REQUESTS,
        ErrorLabel::PROXY_TOO_MANY,
    },
    {
        NS_ERROR_INVALID_CONTENT_ENCODING,
        ErrorLabel::CONTENT_ENCODING,
    },
    {
        NS_ERROR_UNSAFE_CONTENT_TYPE,
        ErrorLabel::UNSAFE_CONTENT,
    },
    {
        NS_ERROR_CORRUPTED_CONTENT,
        ErrorLabel::CORRUPTED_CONTENT,
    },
    {
        NS_ERROR_INTERCEPTION_FAILED,
        ErrorLabel::INTERCEPTION_FAIL,
    },
    {
        NS_ERROR_NET_INADEQUATE_SECURITY,
        ErrorLabel::INADEQUATE_SEC,
    },
    {
        NS_ERROR_BLOCKED_BY_POLICY,
        ErrorLabel::BLOCKED_BY_POLICY,
    },
    {
        NS_ERROR_NET_HTTP2_SENT_GOAWAY,
        ErrorLabel::HTTP2_SENT_GOAWAY,
    },
    {
        NS_ERROR_NET_HTTP3_PROTOCOL_ERROR,
        ErrorLabel::HTTP3_PROTOCOL,
    },
    {
        NS_BINDING_FAILED,
        ErrorLabel::BINDING_FAILED,
    },
};
}  // anonymous namespace

namespace mozilla {
namespace dom {
mozilla::Telemetry::LABELS_PAGE_LOAD_ERROR LoadErrorToTelemetryLabel(
    nsresult aRv) {
  MOZ_ASSERT(aRv != NS_OK);

  for (const auto& p : sResult) {
    if (p.mValue == aRv) {
      return p.mLabel;
    }
  }
  return ErrorLabel::otherError;
}
}  // namespace dom
}  // namespace mozilla
