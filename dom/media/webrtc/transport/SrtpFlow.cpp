/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "logging.h"
#include "SrtpFlow.h"

#include "srtp.h"

#include "transportlayerdtls.h"

#include "mozilla/RefPtr.h"

using namespace mozilla;

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")
bool SrtpFlow::initialized;  // Static

SrtpFlow::~SrtpFlow() {
  if (session_) {
    srtp_dealloc(session_);
  }
}

unsigned int SrtpFlow::KeySize(int cipher_suite) {
  srtp_profile_t profile = static_cast<srtp_profile_t>(cipher_suite);
  return srtp_profile_get_master_key_length(profile);
}

unsigned int SrtpFlow::SaltSize(int cipher_suite) {
  srtp_profile_t profile = static_cast<srtp_profile_t>(cipher_suite);
  return srtp_profile_get_master_salt_length(profile);
}

RefPtr<SrtpFlow> SrtpFlow::Create(int cipher_suite, bool inbound,
                                  const void* key, size_t key_len) {
  nsresult res = Init();
  if (!NS_SUCCEEDED(res)) return nullptr;

  RefPtr<SrtpFlow> flow = new SrtpFlow();

  if (!key) {
    MOZ_MTLOG(ML_ERROR, "Null SRTP key specified");
    return nullptr;
  }

  if ((key_len > SRTP_MAX_KEY_LENGTH) || (key_len < SRTP_MIN_KEY_LENGTH)) {
    MOZ_ASSERT(false, "Invalid SRTP key length");
    return nullptr;
  }

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(srtp_policy_t));

  // Note that we set the same cipher suite for RTP and RTCP
  // since any flow can only have one cipher suite with DTLS-SRTP
  switch (cipher_suite) {
    case kDtlsSrtpAeadAes256Gcm:
      MOZ_MTLOG(ML_DEBUG, "Setting SRTP cipher suite SRTP_AEAD_AES_256_GCM");
      srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
      srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
      break;
    case kDtlsSrtpAeadAes128Gcm:
      MOZ_MTLOG(ML_DEBUG, "Setting SRTP cipher suite SRTP_AEAD_AES_128_GCM");
      srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
      srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
      break;
    case kDtlsSrtpAes128CmHmacSha1_80:
      MOZ_MTLOG(ML_DEBUG,
                "Setting SRTP cipher suite SRTP_AES128_CM_HMAC_SHA1_80");
      srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
      srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
      break;
    case kDtlsSrtpAes128CmHmacSha1_32:
      MOZ_MTLOG(ML_DEBUG,
                "Setting SRTP cipher suite SRTP_AES128_CM_HMAC_SHA1_32");
      srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
      srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(
          &policy.rtcp);  // 80-bit per RFC 5764
      break;              // S 4.1.2.
    default:
      MOZ_MTLOG(ML_ERROR, "Request to set unknown SRTP cipher suite");
      return nullptr;
  }
  // This key is copied into the srtp_t object, so we don't
  // need to keep it.
  policy.key =
      const_cast<unsigned char*>(static_cast<const unsigned char*>(key));
  policy.ssrc.type = inbound ? ssrc_any_inbound : ssrc_any_outbound;
  policy.ssrc.value = 0;
  policy.ekt = nullptr;
  policy.window_size =
      1024;  // Use the Chrome value.  Needs to be revisited.  Default is 128
  policy.allow_repeat_tx = 1;  // Use Chrome value; needed for NACK mode to work
  policy.next = nullptr;

  // Now make the session
  srtp_err_status_t r = srtp_create(&flow->session_, &policy);
  if (r != srtp_err_status_ok) {
    MOZ_MTLOG(ML_ERROR, "Error creating srtp session");
    return nullptr;
  }

  return flow;
}

nsresult SrtpFlow::CheckInputs(bool protect, void* in, int in_len, int max_len,
                               int* out_len) {
  MOZ_ASSERT(in);
  if (!in) {
    MOZ_MTLOG(ML_ERROR, "NULL input value");
    return NS_ERROR_NULL_POINTER;
  }

  if (in_len < 0) {
    MOZ_MTLOG(ML_ERROR, "Input length is negative");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (max_len < 0) {
    MOZ_MTLOG(ML_ERROR, "Max output length is negative");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (protect) {
    if ((max_len < SRTP_MAX_EXPANSION) ||
        ((max_len - SRTP_MAX_EXPANSION) < in_len)) {
      MOZ_MTLOG(ML_ERROR, "Output too short");
      return NS_ERROR_ILLEGAL_VALUE;
    }
  } else {
    if (in_len > max_len) {
      MOZ_MTLOG(ML_ERROR, "Output too short");
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }

  return NS_OK;
}

nsresult SrtpFlow::ProtectRtp(void* in, int in_len, int max_len, int* out_len) {
  nsresult res = CheckInputs(true, in, in_len, max_len, out_len);
  if (NS_FAILED(res)) return res;

  int len = in_len;
  srtp_err_status_t r = srtp_protect(session_, in, &len);

  if (r != srtp_err_status_ok) {
    MOZ_MTLOG(ML_ERROR, "Error protecting SRTP packet");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  MOZ_MTLOG(ML_DEBUG,
            "Successfully protected an SRTP packet of len " << *out_len);

  return NS_OK;
}

nsresult SrtpFlow::UnprotectRtp(void* in, int in_len, int max_len,
                                int* out_len) {
  nsresult res = CheckInputs(false, in, in_len, max_len, out_len);
  if (NS_FAILED(res)) return res;

  int len = in_len;
  srtp_err_status_t r = srtp_unprotect(session_, in, &len);

  if (r != srtp_err_status_ok) {
    MOZ_MTLOG(ML_ERROR, "Error unprotecting SRTP packet error=" << (int)r);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  MOZ_MTLOG(ML_DEBUG,
            "Successfully unprotected an SRTP packet of len " << *out_len);

  return NS_OK;
}

nsresult SrtpFlow::ProtectRtcp(void* in, int in_len, int max_len,
                               int* out_len) {
  nsresult res = CheckInputs(true, in, in_len, max_len, out_len);
  if (NS_FAILED(res)) return res;

  int len = in_len;
  srtp_err_status_t r = srtp_protect_rtcp(session_, in, &len);

  if (r != srtp_err_status_ok) {
    MOZ_MTLOG(ML_ERROR, "Error protecting SRTCP packet");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  MOZ_MTLOG(ML_DEBUG,
            "Successfully protected an SRTCP packet of len " << *out_len);

  return NS_OK;
}

nsresult SrtpFlow::UnprotectRtcp(void* in, int in_len, int max_len,
                                 int* out_len) {
  nsresult res = CheckInputs(false, in, in_len, max_len, out_len);
  if (NS_FAILED(res)) return res;

  int len = in_len;
  srtp_err_status_t r = srtp_unprotect_rtcp(session_, in, &len);

  if (r != srtp_err_status_ok) {
    MOZ_MTLOG(ML_ERROR, "Error unprotecting SRTCP packet error=" << (int)r);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  MOZ_MTLOG(ML_DEBUG,
            "Successfully unprotected an SRTCP packet of len " << *out_len);

  return NS_OK;
}

// Statics
void SrtpFlow::srtp_event_handler(srtp_event_data_t* data) {
  // TODO(ekr@rtfm.com): Implement this
  MOZ_CRASH();
}

nsresult SrtpFlow::Init() {
  if (!initialized) {
    srtp_err_status_t r = srtp_init();
    if (r != srtp_err_status_ok) {
      MOZ_MTLOG(ML_ERROR, "Could not initialize SRTP");
      MOZ_ASSERT(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    r = srtp_install_event_handler(&SrtpFlow::srtp_event_handler);
    if (r != srtp_err_status_ok) {
      MOZ_MTLOG(ML_ERROR, "Could not install SRTP event handler");
      MOZ_ASSERT(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    initialized = true;
  }

  return NS_OK;
}

}  // namespace mozilla
