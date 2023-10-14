/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayerdtls_h__
#define transportlayerdtls_h__

#include <queue>
#include <set>

#ifdef XP_MACOSX
// ensure that Apple Security kit enum goes before "sslproto.h"
#  include <CoreFoundation/CFAvailability.h>
#  include <Security/CipherSuite.h>
#endif

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "ScopedNSSTypes.h"
#include "m_cpp_utils.h"
#include "dtlsidentity.h"
#include "transportlayer.h"
#include "ssl.h"
#include "sslproto.h"

namespace mozilla {

// RFC 5764 (we don't support the NULL cipher)
static const uint16_t kDtlsSrtpAes128CmHmacSha1_80 = 0x0001;
static const uint16_t kDtlsSrtpAes128CmHmacSha1_32 = 0x0002;
// RFC 7714
static const uint16_t kDtlsSrtpAeadAes128Gcm = 0x0007;
static const uint16_t kDtlsSrtpAeadAes256Gcm = 0x0008;

struct Packet;

class TransportLayerNSPRAdapter {
 public:
  explicit TransportLayerNSPRAdapter(TransportLayer* output)
      : output_(output), enabled_(true) {}

  void PacketReceived(MediaPacket& packet);
  int32_t Recv(void* buf, int32_t buflen);
  int32_t Write(const void* buf, int32_t length);
  void SetEnabled(bool enabled) { enabled_ = enabled; }

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerNSPRAdapter);

  TransportLayer* output_;
  std::queue<MediaPacket*> input_;
  bool enabled_;
};

class TransportLayerDtls final : public TransportLayer {
 public:
  TransportLayerDtls() = default;

  virtual ~TransportLayerDtls();

  enum Role { CLIENT, SERVER };
  enum Verification { VERIFY_UNSET, VERIFY_ALLOW_ALL, VERIFY_DIGEST };

  // DTLS-specific operations
  void SetRole(Role role) { role_ = role; }
  Role role() { return role_; }

  enum class Version : uint16_t {
    DTLS_1_0 = SSL_LIBRARY_VERSION_DTLS_1_0,
    DTLS_1_2 = SSL_LIBRARY_VERSION_DTLS_1_2,
    DTLS_1_3 = SSL_LIBRARY_VERSION_DTLS_1_3
  };
  void SetMinMaxVersion(Version min_version, Version max_version);

  void SetIdentity(const RefPtr<DtlsIdentity>& identity) {
    identity_ = identity;
  }
  nsresult SetAlpn(const std::set<std::string>& allowedAlpn,
                   const std::string& alpnDefault);
  const std::string& GetNegotiatedAlpn() const { return alpn_; }

  nsresult SetVerificationAllowAll();

  nsresult SetVerificationDigest(const DtlsDigest& digest);

  nsresult GetCipherSuite(uint16_t* cipherSuite) const;

  nsresult SetSrtpCiphers(const std::vector<uint16_t>& ciphers);
  nsresult GetSrtpCipher(uint16_t* cipher) const;
  static std::vector<uint16_t> GetDefaultSrtpCiphers();

  nsresult ExportKeyingMaterial(const std::string& label, bool use_context,
                                const std::string& context, unsigned char* out,
                                unsigned int outlen);

  // Transport layer overrides.
  nsresult InitInternal() override;
  void WasInserted() override;
  TransportResult SendPacket(MediaPacket& packet) override;

  // Signals
  void StateChange(TransportLayer* layer, State state);
  void PacketReceived(TransportLayer* layer, MediaPacket& packet);

  // For testing use only.  Returns the fd.
  PRFileDesc* internal_fd() {
    CheckThread();
    return ssl_fd_.get();
  }

  TRANSPORT_LAYER_ID("dtls")

 protected:
  void SetState(State state, const char* file, unsigned line) override;

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerDtls);

  bool Setup();
  bool SetupCipherSuites(UniquePRFileDesc& ssl_fd);
  bool SetupAlpn(UniquePRFileDesc& ssl_fd) const;
  void GetDecryptedPackets();
  void Handshake();

  bool CheckAlpn();

  static SECStatus GetClientAuthDataHook(void* arg, PRFileDesc* fd,
                                         CERTDistNames* caNames,
                                         CERTCertificate** pRetCert,
                                         SECKEYPrivateKey** pRetKey);
  static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer);
  SECStatus AuthCertificateHook(PRFileDesc* fd, PRBool checksig,
                                PRBool isServer);

  static void TimerCallback(nsITimer* timer, void* arg);

  SECStatus CheckDigest(const DtlsDigest& digest,
                        UniqueCERTCertificate& cert) const;

  void RecordHandshakeCompletionTelemetry(TransportLayer::State endState);
  void RecordTlsTelemetry();

  static PRBool WriteSrtpXtn(PRFileDesc* fd, SSLHandshakeType message,
                             uint8_t* data, unsigned int* len,
                             unsigned int max_len, void* arg);

  static SECStatus HandleSrtpXtn(PRFileDesc* fd, SSLHandshakeType message,
                                 const uint8_t* data, unsigned int len,
                                 SSLAlertDescription* alert, void* arg);

  RefPtr<DtlsIdentity> identity_;
  // What ALPN identifiers are permitted.
  std::set<std::string> alpn_allowed_;
  // What ALPN identifier is used if ALPN is not supported.
  // The empty string indicates that ALPN is required.
  std::string alpn_default_;
  // What ALPN string was negotiated.
  std::string alpn_;
  std::vector<uint16_t> enabled_srtp_ciphers_;
  uint16_t srtp_cipher_ = 0;

  Role role_ = CLIENT;
  Verification verification_mode_ = VERIFY_UNSET;
  std::vector<DtlsDigest> digests_;

  Version minVersion_ = Version::DTLS_1_0;
  Version maxVersion_ = Version::DTLS_1_2;

  // Must delete nspr_io_adapter after ssl_fd_ b/c ssl_fd_ causes an alert
  // (ssl_fd_ contains an un-owning pointer to nspr_io_adapter_)
  UniquePtr<TransportLayerNSPRAdapter> nspr_io_adapter_ = nullptr;
  UniquePRFileDesc ssl_fd_ = nullptr;

  nsCOMPtr<nsITimer> timer_ = nullptr;
  bool auth_hook_called_ = false;
  bool cert_ok_ = false;
};

}  // namespace mozilla
#endif
