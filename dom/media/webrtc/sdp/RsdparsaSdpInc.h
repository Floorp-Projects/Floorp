/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _RUSTSDPINC_H_
#define _RUSTSDPINC_H_

#include "nsError.h"
#include "mozilla/Maybe.h"

#include <stdint.h>
#include <stdbool.h>

struct BandwidthVec;
struct RustSdpSession;
struct RustSdpError;
struct RustMediaSection;
struct RustAttributeList;
struct StringVec;
struct U8Vec;
struct U32Vec;
struct U16Vec;
struct F32Vec;
struct SsrcVec;
struct RustHeapString;

enum class RustSdpAddressType { kRustAddrIp4, kRustAddrIp6 };

struct StringView {
  const char* buf;
  size_t len;
};

struct RustAddress {
  char ipAddress[50];
  StringView fqdn;
  bool isFqdn;
};

struct RustExplicitlyTypedAddress {
  RustSdpAddressType addressType;
  RustAddress address;
};

struct RustSdpConnection {
  RustExplicitlyTypedAddress addr;
  uint8_t ttl;
  uint64_t amount;
};

struct RustSdpOrigin {
  StringView username;
  uint64_t sessionId;
  uint64_t sessionVersion;
  RustExplicitlyTypedAddress addr;  // TODO address
};

enum class RustSdpMediaValue { kRustAudio, kRustVideo, kRustApplication };

enum class RustSdpProtocolValue {
  kRustRtpSavpf,
  kRustUdpTlsRtpSavp,
  kRustTcpDtlsRtpSavp,
  kRustUdpTlsRtpSavpf,
  kRustTcpDtlsRtpSavpf,
  kRustDtlsSctp,
  kRustUdpDtlsSctp,
  kRustTcpDtlsSctp,
  kRustRtpAvp,
  kRustRtpAvpf,
  kRustRtpSavp,
};

enum class RustSdpFormatType { kRustIntegers, kRustStrings };

enum class RustSdpAttributeFingerprintHashAlgorithm : uint16_t {
  kSha1,
  kSha224,
  kSha256,
  kSha384,
  kSha512,
};

struct RustSdpAttributeFingerprint {
  RustSdpAttributeFingerprintHashAlgorithm hashAlgorithm;
  U8Vec* fingerprint;
};

enum class RustSdpSetup {
  kRustActive,
  kRustActpass,
  kRustHoldconn,
  kRustPassive
};

enum class RustSdpAttributeDtlsMessageType : uint8_t {
  kClient,
  kServer,
};

struct RustSdpAttributeDtlsMessage {
  RustSdpAttributeDtlsMessageType role;
  StringView value;
};

struct RustSdpAttributeSsrc {
  uint32_t id;
  StringView attribute;
  StringView value;
};

enum class RustSdpAttributeSsrcGroupSemantic {
  kRustDup,
  kRustFid,
  kRustFec,
  kRustFecFr,
  kRustSim,
};

struct RustSdpAttributeSsrcGroup {
  RustSdpAttributeSsrcGroupSemantic semantic;
  SsrcVec* ssrcs;
};

struct RustSdpAttributeRtpmap {
  uint8_t payloadType;
  StringView codecName;
  uint32_t frequency;
  uint32_t channels;
};

struct RustSdpAttributeRtcpFb {
  uint32_t payloadType;
  uint32_t feedbackType;
  StringView parameter;
  StringView extra;
};

struct RustSdpAttributeRidParameters {
  uint32_t max_width;
  uint32_t max_height;
  uint32_t max_fps;
  uint32_t max_fs;
  uint32_t max_br;
  uint32_t max_pps;
  StringVec* unknown;
};

struct RustSdpAttributeRid {
  StringView id;
  uint32_t direction;
  U16Vec* formats;
  RustSdpAttributeRidParameters params;
  StringVec* depends;
};

struct RustSdpAttributeImageAttrXYRange {
  uint32_t min;
  uint32_t max;
  uint32_t step;
  U32Vec* discrete_values;
};

struct RustSdpAttributeImageAttrSRange {
  float min;
  float max;
  F32Vec* discrete_values;
};

struct RustSdpAttributeImageAttrPRange {
  float min;
  float max;
};

struct RustSdpAttributeImageAttrSet {
  RustSdpAttributeImageAttrXYRange x;
  RustSdpAttributeImageAttrXYRange y;
  bool has_sar;
  RustSdpAttributeImageAttrSRange sar;
  bool has_par;
  RustSdpAttributeImageAttrPRange par;
  float q;
};

struct RustSdpAttributeImageAttrSetVec;
struct RustSdpAttributeImageAttrSetList {
  RustSdpAttributeImageAttrSetVec* sets;
};

struct RustSdpAttributeImageAttr {
  uint32_t payloadType;
  RustSdpAttributeImageAttrSetList send;
  RustSdpAttributeImageAttrSetList recv;
};

struct RustRtxFmtpParameters {
  uint8_t apt;
  bool has_rtx_time;
  uint32_t rtx_time;
};

struct RustSdpAttributeFmtpParameters {
  // H264
  uint32_t packetization_mode;
  bool level_asymmetry_allowed;
  uint32_t profile_level_id;
  uint32_t max_fs;
  uint32_t max_cpb;
  uint32_t max_dpb;
  uint32_t max_br;
  uint32_t max_mbps;

  // VP8 and VP9
  // max_fs, already defined in H264
  uint32_t max_fr;

  // Opus
  uint32_t maxplaybackrate;
  uint32_t maxaveragebitrate;
  bool usedtx;
  bool stereo;
  bool useinbandfec;
  bool cbr;
  uint32_t ptime;
  uint32_t minptime;
  uint32_t maxptime;

  // telephone-event
  StringView dtmf_tones;

  // RTX
  RustRtxFmtpParameters rtx;

  // Red codecs
  U8Vec* encodings;

  // Unknown
  StringVec* unknown_tokens;
};

struct RustSdpAttributeFmtp {
  uint8_t payloadType;
  StringView codecName;
  RustSdpAttributeFmtpParameters parameters;
};

struct RustSdpAttributeFlags {
  bool iceLite;
  bool rtcpMux;
  bool rtcpRsize;
  bool bundleOnly;
  bool endOfCandidates;
};

struct RustSdpAttributeMsid {
  StringView id;
  StringView appdata;
};

struct RustSdpAttributeMsidSemantic {
  StringView semantic;
  StringVec* msids;
};

enum class RustSdpAttributeGroupSemantic {
  kRustLipSynchronization,
  kRustFlowIdentification,
  kRustSingleReservationFlow,
  kRustAlternateNetworkAddressType,
  kRustForwardErrorCorrection,
  kRustDecodingDependency,
  kRustBundle,
};

struct RustSdpAttributeGroup {
  RustSdpAttributeGroupSemantic semantic;
  StringVec* tags;
};

struct RustSdpAttributeRtcp {
  uint32_t port;
  RustExplicitlyTypedAddress unicastAddr;
  bool has_address;
};

struct RustSdpAttributeSctpmap {
  uint32_t port;
  uint32_t channels;
};

struct RustSdpAttributeSimulcastId {
  StringView id;
  bool paused;
};

struct RustSdpAttributeSimulcastIdVec;
struct RustSdpAttributeSimulcastVersion {
  RustSdpAttributeSimulcastIdVec* ids;
};

struct RustSdpAttributeSimulcastVersionVec;
struct RustSdpAttributeSimulcast {
  RustSdpAttributeSimulcastVersionVec* send;
  RustSdpAttributeSimulcastVersionVec* recv;
};

enum class RustDirection {
  kRustRecvonly,
  kRustSendonly,
  kRustSendrecv,
  kRustInactive
};

struct RustSdpAttributeRemoteCandidate {
  uint32_t component;
  RustAddress address;
  uint32_t port;
};

struct RustSdpAttributeExtmap {
  uint16_t id;
  bool direction_specified;
  RustDirection direction;
  StringView url;
  StringView extensionAttributes;
};

extern "C" {

size_t string_vec_len(const StringVec* vec);
nsresult string_vec_get_view(const StringVec* vec, size_t index,
                             StringView* str);
nsresult free_boxed_string_vec(StringVec* vec);

size_t f32_vec_len(const F32Vec* vec);
nsresult f32_vec_get(const F32Vec* vec, size_t index, float* ret);

size_t u32_vec_len(const U32Vec* vec);
nsresult u32_vec_get(const U32Vec* vec, size_t index, uint32_t* ret);

size_t u16_vec_len(const U16Vec* vec);
nsresult u16_vec_get(const U16Vec* vec, size_t index, uint16_t* ret);

size_t u8_vec_len(const U8Vec* vec);
nsresult u8_vec_get(const U8Vec* vec, size_t index, uint8_t* ret);

size_t ssrc_vec_len(const SsrcVec* vec);
nsresult ssrc_vec_get_id(const SsrcVec* vec, size_t index, uint32_t* ret);

void sdp_free_string(char* string);

nsresult parse_sdp(StringView sdp, bool fail_on_warning, RustSdpSession** ret,
                   RustSdpError** err);
RustSdpSession* sdp_new_reference(RustSdpSession* aSess);
void sdp_free_session(RustSdpSession* ret);
size_t sdp_get_error_line_num(const RustSdpError* err);
char* sdp_get_error_message(const RustSdpError* err);
void sdp_free_error_message(char* message);
void sdp_free_error(RustSdpError* err);

RustSdpOrigin sdp_get_origin(const RustSdpSession* aSess);

uint32_t get_sdp_bandwidth(const RustSdpSession* aSess,
                           const char* aBandwidthType);
BandwidthVec* sdp_get_session_bandwidth_vec(const RustSdpSession* aSess);
BandwidthVec* sdp_get_media_bandwidth_vec(const RustMediaSection* aMediaSec);
char* sdp_serialize_bandwidth(const BandwidthVec* bandwidths);
bool sdp_session_has_connection(const RustSdpSession* aSess);
nsresult sdp_get_session_connection(const RustSdpSession* aSess,
                                    RustSdpConnection* ret);
RustAttributeList* get_sdp_session_attributes(const RustSdpSession* aSess);

size_t sdp_media_section_count(const RustSdpSession* aSess);
RustMediaSection* sdp_get_media_section(const RustSdpSession* aSess,
                                        size_t aLevel);
nsresult sdp_add_media_section(RustSdpSession* aSess, uint32_t aMediaType,
                               uint32_t aDirection, uint32_t aPort,
                               uint32_t aProtocol, uint32_t aAddrType,
                               StringView aAddr);
RustSdpMediaValue sdp_rust_get_media_type(const RustMediaSection* aMediaSec);
RustSdpProtocolValue sdp_get_media_protocol(const RustMediaSection* aMediaSec);
RustSdpFormatType sdp_get_format_type(const RustMediaSection* aMediaSec);
StringVec* sdp_get_format_string_vec(const RustMediaSection* aMediaSec);
U32Vec* sdp_get_format_u32_vec(const RustMediaSection* aMediaSec);
void sdp_set_media_port(const RustMediaSection* aMediaSec, uint32_t aPort);
uint32_t sdp_get_media_port(const RustMediaSection* aMediaSec);
uint32_t sdp_get_media_port_count(const RustMediaSection* aMediaSec);
uint32_t sdp_get_media_bandwidth(const RustMediaSection* aMediaSec,
                                 const char* aBandwidthType);
bool sdp_media_has_connection(const RustMediaSection* aMediaSec);
nsresult sdp_get_media_connection(const RustMediaSection* aMediaSec,
                                  RustSdpConnection* ret);

RustAttributeList* sdp_get_media_attribute_list(
    const RustMediaSection* aMediaSec);

nsresult sdp_media_add_codec(const RustMediaSection* aMediaSec, uint8_t aPT,
                             StringView aCodecName, uint32_t aClockrate,
                             uint16_t channels);
void sdp_media_clear_codecs(const RustMediaSection* aMediaSec);
nsresult sdp_media_add_datachannel(const RustMediaSection* aMediaSec,
                                   StringView aName, uint16_t aPort,
                                   uint16_t streams, uint32_t aMessageSize);

nsresult sdp_get_iceufrag(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_icepwd(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_identity(const RustAttributeList* aList, StringView* ret);
nsresult sdp_get_iceoptions(const RustAttributeList* aList, StringVec** ret);

nsresult sdp_get_dtls_message(const RustAttributeList* aList,
                              RustSdpAttributeDtlsMessage* ret);

size_t sdp_get_fingerprint_count(const RustAttributeList* aList);
void sdp_get_fingerprints(const RustAttributeList* aList, size_t listSize,
                          RustSdpAttributeFingerprint* ret);

nsresult sdp_get_setup(const RustAttributeList* aList, RustSdpSetup* ret);

size_t sdp_get_ssrc_count(const RustAttributeList* aList);
void sdp_get_ssrcs(const RustAttributeList* aList, size_t listSize,
                   RustSdpAttributeSsrc* ret);

size_t sdp_get_ssrc_group_count(const RustAttributeList* aList);
void sdp_get_ssrc_groups(const RustAttributeList* aList, size_t listSize,
                         RustSdpAttributeSsrcGroup* ret);

size_t sdp_get_rtpmap_count(const RustAttributeList* aList);
void sdp_get_rtpmaps(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeRtpmap* ret);

size_t sdp_get_fmtp_count(const RustAttributeList* aList);
size_t sdp_get_fmtp(const RustAttributeList* aList, size_t listSize,
                    RustSdpAttributeFmtp* ret);

int64_t sdp_get_ptime(const RustAttributeList* aList);
int64_t sdp_get_max_msg_size(const RustAttributeList* aList);
int64_t sdp_get_sctp_port(const RustAttributeList* aList);
nsresult sdp_get_maxptime(const RustAttributeList* aList, uint64_t* aMaxPtime);

RustSdpAttributeFlags sdp_get_attribute_flags(const RustAttributeList* aList);

nsresult sdp_get_mid(const RustAttributeList* aList, StringView* ret);

size_t sdp_get_msid_count(const RustAttributeList* aList);
void sdp_get_msids(const RustAttributeList* aList, size_t listSize,
                   RustSdpAttributeMsid* ret);

size_t sdp_get_msid_semantic_count(RustAttributeList* aList);
void sdp_get_msid_semantics(const RustAttributeList* aList, size_t listSize,
                            RustSdpAttributeMsidSemantic* ret);

size_t sdp_get_group_count(const RustAttributeList* aList);
void sdp_get_groups(const RustAttributeList* aList, size_t listSize,
                    RustSdpAttributeGroup* ret);

nsresult sdp_get_rtcp(const RustAttributeList* aList,
                      RustSdpAttributeRtcp* ret);

size_t sdp_get_rtcpfb_count(const RustAttributeList* aList);
void sdp_get_rtcpfbs(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeRtcpFb* ret);

size_t sdp_get_imageattr_count(const RustAttributeList* aList);
void sdp_get_imageattrs(const RustAttributeList* aList, size_t listSize,
                        RustSdpAttributeImageAttr* ret);

size_t sdp_imageattr_get_set_count(const RustSdpAttributeImageAttrSetVec* sets);
void sdp_imageattr_get_sets(const RustSdpAttributeImageAttrSetVec* sets,
                            size_t listSize, RustSdpAttributeImageAttrSet* ret);

size_t sdp_get_sctpmap_count(const RustAttributeList* aList);
void sdp_get_sctpmaps(const RustAttributeList* aList, size_t listSize,
                      RustSdpAttributeSctpmap* ret);

nsresult sdp_get_simulcast(const RustAttributeList* aList,
                           RustSdpAttributeSimulcast* ret);

size_t sdp_simulcast_get_version_count(
    const RustSdpAttributeSimulcastVersionVec* aVersionList);
void sdp_simulcast_get_versions(
    const RustSdpAttributeSimulcastVersionVec* aversionList, size_t listSize,
    RustSdpAttributeSimulcastVersion* ret);

size_t sdp_simulcast_get_ids_count(const RustSdpAttributeSimulcastIdVec* aAlts);
void sdp_simulcast_get_ids(const RustSdpAttributeSimulcastIdVec* aAlts,
                           size_t listSize, RustSdpAttributeSimulcastId* ret);

RustDirection sdp_get_direction(const RustAttributeList* aList);

size_t sdp_get_remote_candidate_count(const RustAttributeList* aList);
void sdp_get_remote_candidates(const RustAttributeList* aList, size_t listSize,
                               RustSdpAttributeRemoteCandidate* ret);

size_t sdp_get_candidate_count(const RustAttributeList* aList);
void sdp_get_candidates(const RustAttributeList* aLisst, size_t listSize,
                        StringVec** ret);

size_t sdp_get_rid_count(const RustAttributeList* aList);
void sdp_get_rids(const RustAttributeList* aList, size_t listSize,
                  RustSdpAttributeRid* ret);

size_t sdp_get_extmap_count(const RustAttributeList* aList);
void sdp_get_extmaps(const RustAttributeList* aList, size_t listSize,
                     RustSdpAttributeExtmap* ret);

}  // extern "C"

#endif
