
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <functional>

#ifdef XP_MACOSX
// ensure that Apple Security kit enum goes before "sslproto.h"
#  include <CoreFoundation/CFAvailability.h>
#  include <Security/CipherSuite.h>
#endif

#include "mozilla/UniquePtr.h"

#include "sigslot.h"

#include "logging.h"
#include "ssl.h"
#include "sslexp.h"
#include "sslproto.h"

#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "mediapacket.h"
#include "dtlsidentity.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"
#include "transportlayerlog.h"
#include "transportlayerloopback.h"

#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mtransport")

const uint8_t kTlsChangeCipherSpecType = 0x14;
const uint8_t kTlsHandshakeType = 0x16;

const uint8_t kTlsHandshakeCertificate = 0x0b;
const uint8_t kTlsHandshakeServerKeyExchange = 0x0c;

const uint8_t kTlsFakeChangeCipherSpec[] = {
    kTlsChangeCipherSpecType,  // Type
    0xfe,
    0xff,  // Version
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,  // Fictitious sequence #
    0x00,
    0x01,  // Length
    0x01   // Value
};

// Layer class which can't be initialized.
class TransportLayerDummy : public TransportLayer {
 public:
  TransportLayerDummy(bool allow_init, bool* destroyed)
      : allow_init_(allow_init), destroyed_(destroyed) {
    *destroyed_ = false;
  }

  virtual ~TransportLayerDummy() { *destroyed_ = true; }

  nsresult InitInternal() override {
    return allow_init_ ? NS_OK : NS_ERROR_FAILURE;
  }

  TransportResult SendPacket(MediaPacket& packet) override {
    MOZ_CRASH();  // Should never be called.
    return 0;
  }

  TRANSPORT_LAYER_ID("lossy")

 private:
  bool allow_init_;
  bool* destroyed_;
};

class Inspector {
 public:
  virtual ~Inspector() = default;

  virtual void Inspect(TransportLayer* layer, const unsigned char* data,
                       size_t len) = 0;
};

// Class to simulate various kinds of network lossage
class TransportLayerLossy : public TransportLayer {
 public:
  TransportLayerLossy() : loss_mask_(0), packet_(0), inspector_(nullptr) {}
  ~TransportLayerLossy() = default;

  TransportResult SendPacket(MediaPacket& packet) override {
    MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "SendPacket(" << packet.len() << ")");

    if (loss_mask_ & (1 << (packet_ % 32))) {
      MOZ_MTLOG(ML_NOTICE, "Dropping packet");
      ++packet_;
      return packet.len();
    }
    if (inspector_) {
      inspector_->Inspect(this, packet.data(), packet.len());
    }

    ++packet_;

    return downward_->SendPacket(packet);
  }

  void SetLoss(uint32_t packet) { loss_mask_ |= (1 << (packet & 32)); }

  void SetInspector(UniquePtr<Inspector> inspector) {
    inspector_ = std::move(inspector);
  }

  void StateChange(TransportLayer* layer, State state) { TL_SET_STATE(state); }

  void PacketReceived(TransportLayer* layer, MediaPacket& packet) {
    SignalPacketReceived(this, packet);
  }

  TRANSPORT_LAYER_ID("lossy")

 protected:
  void WasInserted() override {
    downward_->SignalPacketReceived.connect(
        this, &TransportLayerLossy::PacketReceived);
    downward_->SignalStateChange.connect(this,
                                         &TransportLayerLossy::StateChange);

    TL_SET_STATE(downward_->state());
  }

 private:
  uint32_t loss_mask_;
  uint32_t packet_;
  UniquePtr<Inspector> inspector_;
};

// Process DTLS Records
#define CHECK_LENGTH(expected)                \
  do {                                        \
    EXPECT_GE(remaining(), expected);         \
    if (remaining() < expected) return false; \
  } while (0)

class TlsParser {
 public:
  TlsParser(const unsigned char* data, size_t len) : buffer_(), offset_(0) {
    buffer_.Copy(data, len);
  }

  bool Read(unsigned char* val) {
    if (remaining() < 1) {
      return false;
    }
    *val = *ptr();
    consume(1);
    return true;
  }

  // Read an integral type of specified width.
  bool Read(uint32_t* val, size_t len) {
    if (len > sizeof(uint32_t)) return false;

    *val = 0;

    for (size_t i = 0; i < len; ++i) {
      unsigned char tmp;

      if (!Read(&tmp)) return false;

      (*val) = ((*val) << 8) + tmp;
    }

    return true;
  }

  bool Read(unsigned char* val, size_t len) {
    if (remaining() < len) {
      return false;
    }

    if (val) {
      memcpy(val, ptr(), len);
    }
    consume(len);

    return true;
  }

 private:
  size_t remaining() const { return buffer_.len() - offset_; }
  const uint8_t* ptr() const { return buffer_.data() + offset_; }
  void consume(size_t len) { offset_ += len; }

  MediaPacket buffer_;
  size_t offset_;
};

class DtlsRecordParser {
 public:
  DtlsRecordParser(const unsigned char* data, size_t len)
      : buffer_(), offset_(0) {
    buffer_.Copy(data, len);
  }

  bool NextRecord(uint8_t* ct, UniquePtr<MediaPacket>* buffer) {
    if (!remaining()) return false;

    CHECK_LENGTH(13U);
    const uint8_t* ctp = reinterpret_cast<const uint8_t*>(ptr());
    consume(11);  // ct + version + length

    const uint16_t* tmp = reinterpret_cast<const uint16_t*>(ptr());
    size_t length = ntohs(*tmp);
    consume(2);

    CHECK_LENGTH(length);
    auto db = MakeUnique<MediaPacket>();
    db->Copy(ptr(), length);
    consume(length);

    *ct = *ctp;
    *buffer = std::move(db);

    return true;
  }

 private:
  size_t remaining() const { return buffer_.len() - offset_; }
  const uint8_t* ptr() const { return buffer_.data() + offset_; }
  void consume(size_t len) { offset_ += len; }

  MediaPacket buffer_;
  size_t offset_;
};

// Inspector that parses out DTLS records and passes
// them on.
class DtlsRecordInspector : public Inspector {
 public:
  virtual void Inspect(TransportLayer* layer, const unsigned char* data,
                       size_t len) {
    DtlsRecordParser parser(data, len);

    uint8_t ct;
    UniquePtr<MediaPacket> buf;
    while (parser.NextRecord(&ct, &buf)) {
      OnRecord(layer, ct, buf->data(), buf->len());
    }
  }

  virtual void OnRecord(TransportLayer* layer, uint8_t content_type,
                        const unsigned char* record, size_t len) = 0;
};

// Inspector that injects arbitrary packets based on
// DTLS records of various types.
class DtlsInspectorInjector : public DtlsRecordInspector {
 public:
  DtlsInspectorInjector(uint8_t packet_type, uint8_t handshake_type,
                        const unsigned char* data, size_t len)
      : packet_type_(packet_type), handshake_type_(handshake_type) {
    packet_.Copy(data, len);
  }

  virtual void OnRecord(TransportLayer* layer, uint8_t content_type,
                        const unsigned char* data, size_t len) {
    // Only inject once.
    if (!packet_.data()) {
      return;
    }

    // Check that the first byte is as requested.
    if (content_type != packet_type_) {
      return;
    }

    if (handshake_type_ != 0xff) {
      // Check that the packet is plausibly long enough.
      if (len < 1) {
        return;
      }

      // Check that the handshake type is as requested.
      if (data[0] != handshake_type_) {
        return;
      }
    }

    layer->SendPacket(packet_);
    packet_.Reset();
  }

 private:
  uint8_t packet_type_;
  uint8_t handshake_type_;
  MediaPacket packet_;
};

// Make a copy of the first instance of a message.
class DtlsInspectorRecordHandshakeMessage : public DtlsRecordInspector {
 public:
  explicit DtlsInspectorRecordHandshakeMessage(uint8_t handshake_type)
      : handshake_type_(handshake_type), buffer_() {}

  virtual void OnRecord(TransportLayer* layer, uint8_t content_type,
                        const unsigned char* data, size_t len) {
    // Only do this once.
    if (buffer_.len()) {
      return;
    }

    // Check that the first byte is as requested.
    if (content_type != kTlsHandshakeType) {
      return;
    }

    TlsParser parser(data, len);
    unsigned char message_type;
    // Read the handshake message type.
    if (!parser.Read(&message_type)) {
      return;
    }
    if (message_type != handshake_type_) {
      return;
    }

    uint32_t length;
    if (!parser.Read(&length, 3)) {
      return;
    }

    uint32_t message_seq;
    if (!parser.Read(&message_seq, 2)) {
      return;
    }

    uint32_t fragment_offset;
    if (!parser.Read(&fragment_offset, 3)) {
      return;
    }

    uint32_t fragment_length;
    if (!parser.Read(&fragment_length, 3)) {
      return;
    }

    if ((fragment_offset != 0) || (fragment_length != length)) {
      // This shouldn't happen because all current tests where we
      // are using this code don't fragment.
      return;
    }

    UniquePtr<uint8_t[]> buffer(new uint8_t[length]);
    if (!parser.Read(buffer.get(), length)) {
      return;
    }
    buffer_.Take(std::move(buffer), length);
  }

  const MediaPacket& buffer() { return buffer_; }

 private:
  uint8_t handshake_type_;
  MediaPacket buffer_;
};

class TlsServerKeyExchangeECDHE {
 public:
  bool Parse(const unsigned char* data, size_t len) {
    TlsParser parser(data, len);

    uint8_t curve_type;
    if (!parser.Read(&curve_type)) {
      return false;
    }

    if (curve_type != 3) {  // named_curve
      return false;
    }

    uint32_t named_curve;
    if (!parser.Read(&named_curve, 2)) {
      return false;
    }

    uint32_t point_length;
    if (!parser.Read(&point_length, 1)) {
      return false;
    }

    UniquePtr<uint8_t[]> key(new uint8_t[point_length]);
    if (!parser.Read(key.get(), point_length)) {
      return false;
    }
    public_key_.Take(std::move(key), point_length);

    return true;
  }

  MediaPacket public_key_;
};

namespace {
class TransportTestPeer : public sigslot::has_slots<> {
 public:
  TransportTestPeer(nsCOMPtr<nsIEventTarget> target, std::string name,
                    MtransportTestUtils* utils)
      : name_(name),
        offerer_(name == "P1"),
        target_(target),
        received_packets_(0),
        received_bytes_(0),
        flow_(new TransportFlow(name)),
        loopback_(new TransportLayerLoopback()),
        logging_(new TransportLayerLogging()),
        lossy_(new TransportLayerLossy()),
        dtls_(new TransportLayerDtls()),
        identity_(DtlsIdentity::Generate()),
        ice_ctx_(),
        streams_(),
        peer_(nullptr),
        gathering_complete_(false),
        digest_("sha-1"),
        enabled_cipersuites_(),
        disabled_cipersuites_(),
        test_utils_(utils) {
    NrIceCtx::InitializeGlobals(NrIceCtx::GlobalConfig());
    ice_ctx_ = NrIceCtx::Create(name, NrIceCtx::Config());
    std::vector<NrIceStunServer> stun_servers;
    UniquePtr<NrIceStunServer> server(NrIceStunServer::Create(
        std::string((char*)"stun.services.mozilla.com"), 3478));
    stun_servers.push_back(*server);
    EXPECT_TRUE(NS_SUCCEEDED(ice_ctx_->SetStunServers(stun_servers)));

    dtls_->SetIdentity(identity_);
    dtls_->SetRole(offerer_ ? TransportLayerDtls::SERVER
                            : TransportLayerDtls::CLIENT);

    nsresult res = identity_->ComputeFingerprint(&digest_);
    EXPECT_TRUE(NS_SUCCEEDED(res));
    EXPECT_EQ(20u, digest_.value_.size());
  }

  ~TransportTestPeer() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this, &TransportTestPeer::DestroyFlow), NS_DISPATCH_SYNC);
  }

  void DestroyFlow() {
    disconnect_all();
    if (flow_) {
      loopback_->Disconnect();
      flow_ = nullptr;
    }
    ice_ctx_->Destroy();
    ice_ctx_ = nullptr;
    streams_.clear();
  }

  void DisconnectDestroyFlow() {
    test_utils_->sts_target()->Dispatch(
        NS_NewRunnableFunction(
            __func__,
            [this] {
              loopback_->Disconnect();
              disconnect_all();  // Disconnect from the signals;
              flow_ = nullptr;
            }),
        NS_DISPATCH_SYNC);
  }

  void SetDtlsAllowAll() {
    nsresult res = dtls_->SetVerificationAllowAll();
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void SetAlpn(std::string str, bool withDefault, std::string extra = "") {
    std::set<std::string> alpn;
    alpn.insert(str);  // the one we want to select
    if (!extra.empty()) {
      alpn.insert(extra);
    }
    nsresult res = dtls_->SetAlpn(alpn, withDefault ? str : "");
    ASSERT_EQ(NS_OK, res);
  }

  const std::string& GetAlpn() const { return dtls_->GetNegotiatedAlpn(); }

  void SetDtlsPeer(TransportTestPeer* peer, int digests, unsigned int damage) {
    unsigned int mask = 1;

    for (int i = 0; i < digests; i++) {
      DtlsDigest digest_to_set(peer->digest_);

      if (damage & mask) digest_to_set.value_.data()[0]++;

      nsresult res = dtls_->SetVerificationDigest(digest_to_set);

      ASSERT_TRUE(NS_SUCCEEDED(res));

      mask <<= 1;
    }
  }

  void SetupSrtp() {
    std::vector<uint16_t> srtp_ciphers =
        TransportLayerDtls::GetDefaultSrtpCiphers();
    SetSrtpCiphers(srtp_ciphers);
  }

  void SetSrtpCiphers(std::vector<uint16_t>& srtp_ciphers) {
    ASSERT_TRUE(NS_SUCCEEDED(dtls_->SetSrtpCiphers(srtp_ciphers)));
  }

  void ConnectSocket_s(TransportTestPeer* peer) {
    nsresult res;
    res = loopback_->Init();
    ASSERT_EQ((nsresult)NS_OK, res);

    loopback_->Connect(peer->loopback_);
    ASSERT_EQ((nsresult)NS_OK, loopback_->Init());
    ASSERT_EQ((nsresult)NS_OK, logging_->Init());
    ASSERT_EQ((nsresult)NS_OK, lossy_->Init());
    ASSERT_EQ((nsresult)NS_OK, dtls_->Init());
    dtls_->Chain(lossy_);
    lossy_->Chain(logging_);
    logging_->Chain(loopback_);

    flow_->PushLayer(loopback_);
    flow_->PushLayer(logging_);
    flow_->PushLayer(lossy_);
    flow_->PushLayer(dtls_);

    if (dtls_->state() != TransportLayer::TS_ERROR) {
      // Don't execute these blocks if DTLS didn't initialize.
      TweakCiphers(dtls_->internal_fd());
      if (post_setup_) {
        post_setup_(dtls_->internal_fd());
      }
    }

    dtls_->SignalPacketReceived.connect(this,
                                        &TransportTestPeer::PacketReceived);
  }

  void TweakCiphers(PRFileDesc* fd) {
    for (unsigned short& enabled_cipersuite : enabled_cipersuites_) {
      SSL_CipherPrefSet(fd, enabled_cipersuite, PR_TRUE);
    }
    for (unsigned short& disabled_cipersuite : disabled_cipersuites_) {
      SSL_CipherPrefSet(fd, disabled_cipersuite, PR_FALSE);
    }
  }

  void ConnectSocket(TransportTestPeer* peer) {
    RUN_ON_THREAD(test_utils_->sts_target(),
                  WrapRunnable(this, &TransportTestPeer::ConnectSocket_s, peer),
                  NS_DISPATCH_SYNC);
  }

  nsresult InitIce_s() {
    nsresult rv = ice_->Init();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = dtls_->Init();
    NS_ENSURE_SUCCESS(rv, rv);
    dtls_->Chain(ice_);
    flow_->PushLayer(ice_);
    flow_->PushLayer(dtls_);
    return NS_OK;
  }

  void InitIce() {
    nsresult res;

    // Attach our slots
    ice_ctx_->SignalGatheringStateChange.connect(
        this, &TransportTestPeer::GatheringStateChange);

    char name[100];
    snprintf(name, sizeof(name), "%s:stream%d", name_.c_str(),
             (int)streams_.size());

    // Create the media stream
    RefPtr<NrIceMediaStream> stream = ice_ctx_->CreateStream(name, name, 1);

    ASSERT_TRUE(stream != nullptr);
    stream->SetIceCredentials("ufrag", "pass");
    streams_.push_back(stream);

    // Listen for candidates
    stream->SignalCandidate.connect(this, &TransportTestPeer::GotCandidate);

    // Create the transport layer
    ice_ = new TransportLayerIce();
    ice_->SetParameters(stream, 1);

    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, this, &TransportTestPeer::InitIce_s),
        NS_DISPATCH_SYNC);

    ASSERT_EQ((nsresult)NS_OK, res);

    // Listen for media events
    dtls_->SignalPacketReceived.connect(this,
                                        &TransportTestPeer::PacketReceived);
    dtls_->SignalStateChange.connect(this, &TransportTestPeer::StateChanged);

    // Start gathering
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, ice_ctx_, &NrIceCtx::StartGathering, false,
                        false),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void ConnectIce(TransportTestPeer* peer) {
    peer_ = peer;

    // If gathering is already complete, push the candidates over
    if (gathering_complete_) GatheringComplete();
  }

  // New candidate
  void GotCandidate(NrIceMediaStream* stream, const std::string& candidate,
                    const std::string& ufrag, const std::string& mdns_addr,
                    const std::string& actual_addr) {
    std::cerr << "Got candidate " << candidate << " (ufrag=" << ufrag << ")"
              << std::endl;
  }

  void GatheringStateChange(NrIceCtx* ctx, NrIceCtx::GatheringState state) {
    (void)ctx;
    if (state == NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
      GatheringComplete();
    }
  }

  // Gathering complete, so send our candidates and start
  // connecting on the other peer.
  void GatheringComplete() {
    nsresult res;

    // Don't send to the other side
    if (!peer_) {
      gathering_complete_ = true;
      return;
    }

    // First send attributes
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, peer_->ice_ctx_, &NrIceCtx::ParseGlobalAttributes,
                        ice_ctx_->GetGlobalAttributes()),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));

    for (size_t i = 0; i < streams_.size(); ++i) {
      test_utils_->sts_target()->Dispatch(
          WrapRunnableRet(&res, peer_->streams_[i],
                          &NrIceMediaStream::ConnectToPeer, "ufrag", "pass",
                          streams_[i]->GetAttributes()),
          NS_DISPATCH_SYNC);

      ASSERT_TRUE(NS_SUCCEEDED(res));
    }

    // Start checks on the other peer.
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, peer_->ice_ctx_, &NrIceCtx::StartChecks),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  // WrapRunnable/lambda and move semantics (MediaPacket is not copyable) don't
  // get along yet, so we need a wrapper. Gross.
  static TransportResult SendPacketWrapper(TransportLayer* layer,
                                           MediaPacket* packet) {
    return layer->SendPacket(*packet);
  }

  TransportResult SendPacket(MediaPacket& packet) {
    TransportResult ret;

    test_utils_->sts_target()->Dispatch(
        WrapRunnableNMRet(&ret, &TransportTestPeer::SendPacketWrapper, dtls_,
                          &packet),
        NS_DISPATCH_SYNC);

    return ret;
  }

  void StateChanged(TransportLayer* layer, TransportLayer::State state) {
    if (state == TransportLayer::TS_OPEN) {
      std::cerr << "Now connected" << std::endl;
    }
  }

  void PacketReceived(TransportLayer* layer, MediaPacket& packet) {
    std::cerr << "Received " << packet.len() << " bytes" << std::endl;
    ++received_packets_;
    received_bytes_ += packet.len();
  }

  void SetLoss(uint32_t loss) { lossy_->SetLoss(loss); }

  void SetCombinePackets(bool combine) { loopback_->CombinePackets(combine); }

  void SetInspector(UniquePtr<Inspector> inspector) {
    lossy_->SetInspector(std::move(inspector));
  }

  void SetInspector(Inspector* in) {
    UniquePtr<Inspector> inspector(in);

    lossy_->SetInspector(std::move(inspector));
  }

  void SetCipherSuiteChanges(const std::vector<uint16_t>& enableThese,
                             const std::vector<uint16_t>& disableThese) {
    disabled_cipersuites_ = disableThese;
    enabled_cipersuites_ = enableThese;
  }

  void SetPostSetup(const std::function<void(PRFileDesc*)>& setup) {
    post_setup_ = std::move(setup);
  }

  TransportLayer::State state() {
    TransportLayer::State tstate;

    RUN_ON_THREAD(test_utils_->sts_target(),
                  WrapRunnableRet(&tstate, dtls_, &TransportLayer::state));

    return tstate;
  }

  bool connected() { return state() == TransportLayer::TS_OPEN; }

  bool failed() { return state() == TransportLayer::TS_ERROR; }

  size_t receivedPackets() { return received_packets_; }

  size_t receivedBytes() { return received_bytes_; }

  uint16_t cipherSuite() const {
    nsresult rv;
    uint16_t cipher;
    RUN_ON_THREAD(
        test_utils_->sts_target(),
        WrapRunnableRet(&rv, dtls_, &TransportLayerDtls::GetCipherSuite,
                        &cipher));

    if (NS_FAILED(rv)) {
      return TLS_NULL_WITH_NULL_NULL;  // i.e., not good
    }
    return cipher;
  }

  uint16_t srtpCipher() const {
    nsresult rv;
    uint16_t cipher;
    RUN_ON_THREAD(test_utils_->sts_target(),
                  WrapRunnableRet(&rv, dtls_,
                                  &TransportLayerDtls::GetSrtpCipher, &cipher));
    if (NS_FAILED(rv)) {
      return 0;  // the SRTP equivalent of TLS_NULL_WITH_NULL_NULL
    }
    return cipher;
  }

 private:
  std::string name_;
  bool offerer_;
  nsCOMPtr<nsIEventTarget> target_;
  size_t received_packets_;
  size_t received_bytes_;
  RefPtr<TransportFlow> flow_;
  TransportLayerLoopback* loopback_;
  TransportLayerLogging* logging_;
  TransportLayerLossy* lossy_;
  TransportLayerDtls* dtls_;
  TransportLayerIce* ice_;
  RefPtr<DtlsIdentity> identity_;
  RefPtr<NrIceCtx> ice_ctx_;
  std::vector<RefPtr<NrIceMediaStream> > streams_;
  TransportTestPeer* peer_;
  bool gathering_complete_;
  DtlsDigest digest_;
  std::vector<uint16_t> enabled_cipersuites_;
  std::vector<uint16_t> disabled_cipersuites_;
  MtransportTestUtils* test_utils_;
  std::function<void(PRFileDesc* fd)> post_setup_ = nullptr;
};

class TransportTest : public MtransportTest {
 public:
  TransportTest() {
    fds_[0] = nullptr;
    fds_[1] = nullptr;
    p1_ = nullptr;
    p2_ = nullptr;
  }

  void TearDown() override {
    delete p1_;
    delete p2_;

    //    Can't detach these
    //    PR_Close(fds_[0]);
    //    PR_Close(fds_[1]);
    MtransportTest::TearDown();
  }

  void DestroyPeerFlows() {
    p1_->DisconnectDestroyFlow();
    p2_->DisconnectDestroyFlow();
  }

  void SetUp() override {
    MtransportTest::SetUp();

    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    Reset();
  }

  void Reset() {
    if (p1_) {
      delete p1_;
    }
    if (p2_) {
      delete p2_;
    }
    p1_ = new TransportTestPeer(target_, "P1", test_utils_);
    p2_ = new TransportTestPeer(target_, "P2", test_utils_);
  }

  void SetupSrtp() {
    p1_->SetupSrtp();
    p2_->SetupSrtp();
  }

  void SetDtlsPeer(int digests = 1, unsigned int damage = 0) {
    p1_->SetDtlsPeer(p2_, digests, damage);
    p2_->SetDtlsPeer(p1_, digests, damage);
  }

  void SetDtlsAllowAll() {
    p1_->SetDtlsAllowAll();
    p2_->SetDtlsAllowAll();
  }

  void SetAlpn(std::string first, std::string second,
               bool withDefaults = true) {
    if (!first.empty()) {
      p1_->SetAlpn(first, withDefaults, "bogus");
    }
    if (!second.empty()) {
      p2_->SetAlpn(second, withDefaults);
    }
  }

  void CheckAlpn(std::string first, std::string second) {
    ASSERT_EQ(first, p1_->GetAlpn());
    ASSERT_EQ(second, p2_->GetAlpn());
  }

  void ConnectSocket() {
    ConnectSocketInternal();
    ASSERT_TRUE_WAIT(p1_->connected(), 10000);
    ASSERT_TRUE_WAIT(p2_->connected(), 10000);

    ASSERT_EQ(p1_->cipherSuite(), p2_->cipherSuite());
    ASSERT_EQ(p1_->srtpCipher(), p2_->srtpCipher());
  }

  void ConnectSocketExpectFail() {
    ConnectSocketInternal();
    ASSERT_TRUE_WAIT(p1_->failed(), 10000);
    ASSERT_TRUE_WAIT(p2_->failed(), 10000);
  }

  void ConnectSocketExpectState(TransportLayer::State s1,
                                TransportLayer::State s2) {
    ConnectSocketInternal();
    ASSERT_EQ_WAIT(s1, p1_->state(), 10000);
    ASSERT_EQ_WAIT(s2, p2_->state(), 10000);
  }

  void ConnectIce() {
    p1_->InitIce();
    p2_->InitIce();
    p1_->ConnectIce(p2_);
    p2_->ConnectIce(p1_);
    ASSERT_TRUE_WAIT(p1_->connected(), 10000);
    ASSERT_TRUE_WAIT(p2_->connected(), 10000);
  }

  void TransferTest(size_t count, size_t bytes = 1024) {
    unsigned char buf[bytes];

    for (size_t i = 0; i < count; ++i) {
      memset(buf, count & 0xff, sizeof(buf));
      MediaPacket packet;
      packet.Copy(buf, sizeof(buf));
      TransportResult rv = p1_->SendPacket(packet);
      ASSERT_TRUE(rv > 0);
    }

    std::cerr << "Received == " << p2_->receivedPackets() << " packets"
              << std::endl;
    ASSERT_TRUE_WAIT(count == p2_->receivedPackets(), 10000);
    ASSERT_TRUE((count * sizeof(buf)) == p2_->receivedBytes());
  }

 protected:
  void ConnectSocketInternal() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(p1_, &TransportTestPeer::ConnectSocket, p2_),
        NS_DISPATCH_SYNC);
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(p2_, &TransportTestPeer::ConnectSocket, p1_),
        NS_DISPATCH_SYNC);
  }

  PRFileDesc* fds_[2];
  TransportTestPeer* p1_;
  TransportTestPeer* p2_;
  nsCOMPtr<nsIEventTarget> target_;
};

TEST_F(TransportTest, TestNoDtlsVerificationSettings) {
  ConnectSocketExpectFail();
}

static void DisableChaCha(TransportTestPeer* peer) {
  // On ARM, ChaCha20Poly1305 might be preferred; disable it for the tests that
  // want to check the cipher suite.  It doesn't matter which peer disables the
  // suite, disabling on either side has the same effect.
  std::vector<uint16_t> chachaSuites;
  chachaSuites.push_back(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
  chachaSuites.push_back(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
  peer->SetCipherSuiteChanges(std::vector<uint16_t>(), chachaSuites);
}

TEST_F(TransportTest, TestConnect) {
  SetDtlsPeer();
  DisableChaCha(p1_);
  ConnectSocket();

  // check that we got the right suite
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());

  // no SRTP on this one
  ASSERT_EQ(0, p1_->srtpCipher());
}

TEST_F(TransportTest, TestConnectSrtp) {
  SetupSrtp();
  SetDtlsPeer();
  DisableChaCha(p2_);
  ConnectSocket();

  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());

  // SRTP is on with default value
  ASSERT_EQ(kDtlsSrtpAeadAes128Gcm, p1_->srtpCipher());
}

TEST_F(TransportTest, TestConnectDestroyFlowsMainThread) {
  SetDtlsPeer();
  ConnectSocket();
  DestroyPeerFlows();
}

TEST_F(TransportTest, TestConnectAllowAll) {
  SetDtlsAllowAll();
  ConnectSocket();
}

TEST_F(TransportTest, TestConnectAlpn) {
  SetDtlsPeer();
  SetAlpn("a", "a");
  ConnectSocket();
  CheckAlpn("a", "a");
}

TEST_F(TransportTest, TestConnectAlpnMismatch) {
  SetDtlsPeer();
  SetAlpn("something", "different");
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectAlpnServerDefault) {
  SetDtlsPeer();
  SetAlpn("def", "");
  // server allows default, client doesn't support
  ConnectSocket();
  CheckAlpn("def", "");
}

TEST_F(TransportTest, TestConnectAlpnClientDefault) {
  SetDtlsPeer();
  SetAlpn("", "clientdef");
  // client allows default, but server will ignore the extension
  ConnectSocket();
  CheckAlpn("", "clientdef");
}

TEST_F(TransportTest, TestConnectClientNoAlpn) {
  SetDtlsPeer();
  // Here the server has ALPN, but no default is allowed.
  // Reminder: p1 == server, p2 == client
  SetAlpn("server-nodefault", "", false);
  // The server doesn't see the extension, so negotiates without it.
  // But then the server is forced to close when it discovers that ALPN wasn't
  // negotiated; the client sees a close.
  ConnectSocketExpectState(TransportLayer::TS_ERROR, TransportLayer::TS_CLOSED);
}

TEST_F(TransportTest, TestConnectServerNoAlpn) {
  SetDtlsPeer();
  SetAlpn("", "client-nodefault", false);
  // The client aborts; the server doesn't realize this is a problem and just
  // sees the close.
  ConnectSocketExpectState(TransportLayer::TS_CLOSED, TransportLayer::TS_ERROR);
}

TEST_F(TransportTest, TestConnectNoDigest) {
  SetDtlsPeer(0, 0);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectBadDigest) {
  SetDtlsPeer(1, 1);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectTwoDigests) {
  SetDtlsPeer(2, 0);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsFirstBad) {
  SetDtlsPeer(2, 1);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsSecondBad) {
  SetDtlsPeer(2, 2);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsBothBad) {
  SetDtlsPeer(2, 3);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectInjectCCS) {
  SetDtlsPeer();
  p2_->SetInspector(MakeUnique<DtlsInspectorInjector>(
      kTlsHandshakeType, kTlsHandshakeCertificate, kTlsFakeChangeCipherSpec,
      sizeof(kTlsFakeChangeCipherSpec)));

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectVerifyNewECDHE) {
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage* i1 =
      new DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i1);
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  Reset();
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage* i2 =
      new DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i2);
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Now compare these two to see if they are the same.
  ASSERT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

TEST_F(TransportTest, TestConnectVerifyReusedECDHE) {
  auto set_reuse_ecdhe_key = [](PRFileDesc* fd) {
    // TransportLayerDtls automatically sets this pref to false
    // so set it back for test.
    // This is pretty gross. Dig directly into the NSS FD. The problem
    // is that we are testing a feature which TransaportLayerDtls doesn't
    // expose.
    SECStatus rv = SSL_OptionSet(fd, SSL_REUSE_SERVER_ECDHE_KEY, PR_TRUE);
    ASSERT_EQ(SECSuccess, rv);
  };

  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage* i1 =
      new DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i1);
  p1_->SetPostSetup(set_reuse_ecdhe_key);
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  Reset();
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage* i2 =
      new DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);

  p1_->SetInspector(i2);
  p1_->SetPostSetup(set_reuse_ecdhe_key);

  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Now compare these two to see if they are the same.
  ASSERT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  ASSERT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

TEST_F(TransportTest, TestTransfer) {
  SetDtlsPeer();
  ConnectSocket();
  TransferTest(1);
}

TEST_F(TransportTest, TestTransferMaxSize) {
  SetDtlsPeer();
  ConnectSocket();
  /* transportlayerdtls uses a 9216 bytes buffer - as this test uses the
   * loopback implementation it does not have to take into account the extra
   * bytes added by the DTLS layer below. */
  TransferTest(1, 9216);
}

TEST_F(TransportTest, TestTransferMultiple) {
  SetDtlsPeer();
  ConnectSocket();
  TransferTest(3);
}

TEST_F(TransportTest, TestTransferCombinedPackets) {
  SetDtlsPeer();
  ConnectSocket();
  p2_->SetCombinePackets(true);
  TransferTest(3);
}

TEST_F(TransportTest, TestConnectLoseFirst) {
  SetDtlsPeer();
  p1_->SetLoss(0);
  ConnectSocket();
  TransferTest(1);
}

TEST_F(TransportTest, TestConnectIce) {
  SetDtlsPeer();
  ConnectIce();
}

TEST_F(TransportTest, TestTransferIceMaxSize) {
  SetDtlsPeer();
  ConnectIce();
  /* nICEr and transportlayerdtls both use 9216 bytes buffers. But the DTLS
   * layer add extra bytes to the packet, which size depends on chosen cipher
   * etc. Sending more then 9216 bytes works, but on the receiving side the call
   * to PR_recvfrom() will truncate any packet bigger then nICEr's buffer size
   * of 9216 bytes, which then results in the DTLS layer discarding the packet.
   * Therefore we leave some headroom (according to
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1214269#c29 256 bytes should
   * be save choice) here for the DTLS bytes to make it safely into the
   * receiving buffer in nICEr. */
  TransferTest(1, 8960);
}

TEST_F(TransportTest, TestTransferIceMultiple) {
  SetDtlsPeer();
  ConnectIce();
  TransferTest(3);
}

TEST_F(TransportTest, TestTransferIceCombinedPackets) {
  SetDtlsPeer();
  ConnectIce();
  p2_->SetCombinePackets(true);
  TransferTest(3);
}

// test the default configuration against a peer that supports only
// one of the mandatory-to-implement suites, which should succeed
static void ConfigureOneCipher(TransportTestPeer* peer, uint16_t suite) {
  std::vector<uint16_t> justOne;
  justOne.push_back(suite);
  std::vector<uint16_t> everythingElse(
      SSL_GetImplementedCiphers(),
      SSL_GetImplementedCiphers() + SSL_GetNumImplementedCiphers());
  std::remove(everythingElse.begin(), everythingElse.end(), suite);
  peer->SetCipherSuiteChanges(justOne, everythingElse);
}

TEST_F(TransportTest, TestCipherMismatch) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
  ConfigureOneCipher(p2_, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestCipherMandatoryOnlyGcm) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
  ConnectSocket();
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());
}

TEST_F(TransportTest, TestCipherMandatoryOnlyCbc) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
  ConnectSocket();
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, p1_->cipherSuite());
}

TEST_F(TransportTest, TestSrtpMismatch) {
  std::vector<uint16_t> setA;
  setA.push_back(kDtlsSrtpAes128CmHmacSha1_80);
  std::vector<uint16_t> setB;
  setB.push_back(kDtlsSrtpAes128CmHmacSha1_32);

  p1_->SetSrtpCiphers(setA);
  p2_->SetSrtpCiphers(setB);
  SetDtlsPeer();
  ConnectSocketExpectFail();

  ASSERT_EQ(0, p1_->srtpCipher());
  ASSERT_EQ(0, p2_->srtpCipher());
}

static SECStatus NoopXtnHandler(PRFileDesc* fd, SSLHandshakeType message,
                                const uint8_t* data, unsigned int len,
                                SSLAlertDescription* alert, void* arg) {
  return SECSuccess;
}

static PRBool WriteFixedXtn(PRFileDesc* fd, SSLHandshakeType message,
                            uint8_t* data, unsigned int* len,
                            unsigned int max_len, void* arg) {
  // When we enable TLS 1.3, change ssl_hs_server_hello here to
  // ssl_hs_encrypted_extensions.  At the same time, add a test that writes to
  // ssl_hs_server_hello, which should fail.
  if (message != ssl_hs_client_hello && message != ssl_hs_server_hello) {
    return false;
  }

  auto v = reinterpret_cast<std::vector<uint8_t>*>(arg);
  memcpy(data, &((*v)[0]), v->size());
  *len = v->size();
  return true;
}

// Note that |value| needs to be readable after this function returns.
static void InstallBadSrtpExtensionWriter(TransportTestPeer* peer,
                                          std::vector<uint8_t>* value) {
  peer->SetPostSetup([value](PRFileDesc* fd) {
    // Override the handler that is installed by the DTLS setup.
    SECStatus rv = SSL_InstallExtensionHooks(
        fd, ssl_use_srtp_xtn, WriteFixedXtn, value, NoopXtnHandler, nullptr);
    ASSERT_EQ(SECSuccess, rv);
  });
}

TEST_F(TransportTest, TestSrtpErrorServerSendsTwoSrtpCiphers) {
  // Server (p1_) sends an extension with two values, and empty MKI.
  std::vector<uint8_t> xtn = {0x04, 0x00, 0x01, 0x00, 0x02, 0x00};
  InstallBadSrtpExtensionWriter(p1_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestSrtpErrorServerSendsTwoMki) {
  // Server (p1_) sends an MKI.
  std::vector<uint8_t> xtn = {0x02, 0x00, 0x01, 0x01, 0x00};
  InstallBadSrtpExtensionWriter(p1_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestSrtpErrorServerSendsUnknownValue) {
  std::vector<uint8_t> xtn = {0x02, 0x9a, 0xf1, 0x00};
  InstallBadSrtpExtensionWriter(p1_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestSrtpErrorServerSendsOverflow) {
  std::vector<uint8_t> xtn = {0x32, 0x00, 0x01, 0x00};
  InstallBadSrtpExtensionWriter(p1_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestSrtpErrorServerSendsUnevenList) {
  std::vector<uint8_t> xtn = {0x01, 0x00, 0x00};
  InstallBadSrtpExtensionWriter(p1_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestSrtpErrorClientSendsUnevenList) {
  std::vector<uint8_t> xtn = {0x01, 0x00, 0x00};
  InstallBadSrtpExtensionWriter(p2_, &xtn);
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, OnlyServerSendsSrtpXtn) {
  p1_->SetupSrtp();
  SetDtlsPeer();
  // This should connect, but with no SRTP extension neogtiated.
  // The client side might negotiate a data channel only.
  ConnectSocket();
  ASSERT_NE(TLS_NULL_WITH_NULL_NULL, p1_->cipherSuite());
  ASSERT_EQ(0, p1_->srtpCipher());
}

TEST_F(TransportTest, OnlyClientSendsSrtpXtn) {
  p2_->SetupSrtp();
  SetDtlsPeer();
  // This should connect, but with no SRTP extension neogtiated.
  // The server side might negotiate a data channel only.
  ConnectSocket();
  ASSERT_NE(TLS_NULL_WITH_NULL_NULL, p1_->cipherSuite());
  ASSERT_EQ(0, p1_->srtpCipher());
}

class TransportSrtpParameterTest
    : public TransportTest,
      public ::testing::WithParamInterface<uint16_t> {};

INSTANTIATE_TEST_CASE_P(
    SrtpParamInit, TransportSrtpParameterTest,
    ::testing::ValuesIn(TransportLayerDtls::GetDefaultSrtpCiphers()));

TEST_P(TransportSrtpParameterTest, TestSrtpCiphersMismatchCombinations) {
  uint16_t cipher = GetParam();
  std::cerr << "Checking cipher: " << cipher << std::endl;

  p1_->SetupSrtp();

  std::vector<uint16_t> setB;
  setB.push_back(cipher);

  p2_->SetSrtpCiphers(setB);
  SetDtlsPeer();
  ConnectSocket();

  ASSERT_EQ(cipher, p1_->srtpCipher());
  ASSERT_EQ(cipher, p2_->srtpCipher());
}

// NSS doesn't support DHE suites on the server end.
// This checks to see if we barf when that's the only option available.
TEST_F(TransportTest, TestDheOnlyFails) {
  SetDtlsPeer();

  // p2_ is the client
  // setting this on p1_ (the server) causes NSS to assert
  ConfigureOneCipher(p2_, TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  ConnectSocketExpectFail();
}

}  // end namespace
