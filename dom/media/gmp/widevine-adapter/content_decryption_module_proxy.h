// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_PROXY_H_
#define CDM_CONTENT_DECRYPTION_MODULE_PROXY_H_

#include "content_decryption_module_export.h"

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#else
#  include <stdint.h>
#endif

namespace cdm {

class CDM_CLASS_API CdmProxyClient;

// A proxy class for the CDM.
// In general, the interpretation of the CdmProxy and CdmProxyClient method
// parameters are protocol dependent. For enum parameters, values outside the
// enum range may not work.
class CDM_CLASS_API CdmProxy {
 public:
  enum Function : uint32_t {
    // For Intel Negotiate Crypto SessionKey Exchange (CSME) path to call
    // ID3D11VideoContext::NegotiateCryptoSessionKeyExchange.
    kIntelNegotiateCryptoSessionKeyExchange = 1,
    // There will be more values in the future e.g. for D3D11 RSA method.
  };

  enum KeyType : uint32_t {
    kDecryptOnly = 0,
    kDecryptAndDecode = 1,
  };

  // Initializes the proxy. The results will be returned in
  // CdmProxyClient::OnInitialized().
  virtual void Initialize() = 0;

  // Processes and updates the state of the proxy.
  // |output_data_size| is required by some protocol to set up the output data.
  // The operation may fail if the |output_data_size| is wrong. The results will
  // be returned in CdmProxyClient::OnProcessed().
  virtual void Process(Function function, uint32_t crypto_session_id,
                       const uint8_t* input_data, uint32_t input_data_size,
                       uint32_t output_data_size) = 0;

  // Creates a crypto session for handling media.
  // If extra data has to be passed to further setup the media crypto session,
  // pass the data as |input_data|. The results will be returned in
  // CdmProxyClient::OnMediaCryptoSessionCreated().
  virtual void CreateMediaCryptoSession(const uint8_t* input_data,
                                        uint32_t input_data_size) = 0;

  // Sets a key for the session identified by |crypto_session_id|.
  virtual void SetKey(uint32_t crypto_session_id, const uint8_t* key_id,
                      uint32_t key_id_size, KeyType key_type,
                      const uint8_t* key_blob, uint32_t key_blob_size) = 0;

  // Removes a key for the session identified by |crypto_session_id|.
  virtual void RemoveKey(uint32_t crypto_session_id, const uint8_t* key_id,
                         uint32_t key_id_size) = 0;

 protected:
  CdmProxy() {}
  virtual ~CdmProxy() {}
};

// Responses to CdmProxy calls. All responses will be called asynchronously.
class CDM_CLASS_API CdmProxyClient {
 public:
  enum Status : uint32_t {
    kOk,
    kFail,
  };

  enum Protocol : uint32_t {
    kNone = 0,  // No protocol supported. Can be used in failure cases.
    kIntel,     // Method using Intel CSME.
    // There will be more values in the future e.g. kD3D11RsaHardware,
    // kD3D11RsaSoftware to use the D3D11 RSA method.
  };

  // Callback for Initialize(). If the proxy created a crypto session, then the
  // ID for the crypto session is |crypto_session_id|.
  virtual void OnInitialized(Status status, Protocol protocol,
                             uint32_t crypto_session_id) = 0;

  // Callback for Process(). |output_data| is the output of processing.
  virtual void OnProcessed(Status status, const uint8_t* output_data,
                           uint32_t output_data_size) = 0;

  // Callback for CreateMediaCryptoSession(). On success:
  // - |crypto_session_id| is the ID for the created crypto session.
  // - |output_data| is extra value, if any.
  // Otherwise, |crypto_session_id| and |output_data| should be ignored.
  virtual void OnMediaCryptoSessionCreated(Status status,
                                           uint32_t crypto_session_id,
                                           uint64_t output_data) = 0;

  // Callback for SetKey().
  virtual void OnKeySet(Status status) = 0;

  // Callback for RemoveKey().
  virtual void OnKeyRemoved(Status status) = 0;

  // Called when there is a hardware reset and all the hardware context is lost.
  virtual void NotifyHardwareReset() = 0;

 protected:
  CdmProxyClient() {}
  virtual ~CdmProxyClient() {}
};

}  // namespace cdm

#endif  // CDM_CONTENT_DECRYPTION_MODULE_PROXY_H_
