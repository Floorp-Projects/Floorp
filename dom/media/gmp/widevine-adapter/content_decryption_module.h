// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_H_
#define CDM_CONTENT_DECRYPTION_MODULE_H_

#include <type_traits>

#include "content_decryption_module_export.h"
#include "content_decryption_module_proxy.h"

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
#else
#  include <stdint.h>
#endif

// The version number must be rolled when the exported functions are updated!
// If the CDM and the adapter use different versions of these functions, the
// adapter will fail to load or crash!
#define CDM_MODULE_VERSION 4

// Build the versioned entrypoint name.
// The extra macros are necessary to expand version to an actual value.
#define INITIALIZE_CDM_MODULE \
  BUILD_ENTRYPOINT(InitializeCdmModule, CDM_MODULE_VERSION)
#define BUILD_ENTRYPOINT(name, version) \
  BUILD_ENTRYPOINT_NO_EXPANSION(name, version)
#define BUILD_ENTRYPOINT_NO_EXPANSION(name, version) name##_##version

// Macro to check that |type| does the following:
// 1. is a standard layout.
// 2. is trivial.
// 3. sizeof(type) matches the expected size in bytes. As some types contain
//    pointers, the size is specified for both 32 and 64 bit.
#define CHECK_TYPE(type, size_32, size_64)                           \
  static_assert(std::is_standard_layout<type>(),                     \
                #type " not standard_layout");                       \
  static_assert(std::is_trivial<type>(), #type " not trivial");      \
  static_assert((sizeof(void*) == 4 && sizeof(type) == size_32) ||   \
                    (sizeof(void*) == 8 && sizeof(type) == size_64), \
                #type " size mismatch")

extern "C" {

CDM_API void INITIALIZE_CDM_MODULE();

CDM_API void DeinitializeCdmModule();

// Returns a pointer to the requested CDM Host interface upon success.
// Returns NULL if the requested CDM Host interface is not supported.
// The caller should cast the returned pointer to the type matching
// |host_interface_version|.
typedef void* (*GetCdmHostFunc)(int host_interface_version, void* user_data);

// Returns a pointer to the requested CDM upon success.
// Returns NULL if an error occurs or the requested |cdm_interface_version| or
// |key_system| is not supported or another error occurs.
// The caller should cast the returned pointer to the type matching
// |cdm_interface_version|.
// Caller retains ownership of arguments and must call Destroy() on the returned
// object.
CDM_API void* CreateCdmInstance(int cdm_interface_version,
                                const char* key_system,
                                uint32_t key_system_size,
                                GetCdmHostFunc get_cdm_host_func,
                                void* user_data);

CDM_API const char* GetCdmVersion();

}  // extern "C"

namespace cdm {

enum Status : uint32_t {
  kSuccess = 0,
  kNeedMoreData,  // Decoder needs more data to produce a decoded frame/sample.
  kNoKey,         // The required decryption key is not available.
  kInitializationError,    // Initialization error.
  kDecryptError,           // Decryption failed.
  kDecodeError,            // Error decoding audio or video.
  kDeferredInitialization  // Decoder is not ready for initialization.
};
CHECK_TYPE(Status, 4, 4);

// Exceptions used by the CDM to reject promises.
// https://w3c.github.io/encrypted-media/#exceptions
enum Exception : uint32_t {
  kExceptionTypeError,
  kExceptionNotSupportedError,
  kExceptionInvalidStateError,
  kExceptionQuotaExceededError
};
CHECK_TYPE(Exception, 4, 4);

// The encryption scheme. The definitions are from ISO/IEC 23001-7:2016.
enum class EncryptionScheme : uint32_t {
  kUnencrypted = 0,
  kCenc,  // 'cenc' subsample encryption using AES-CTR mode.
  kCbcs   // 'cbcs' pattern encryption using AES-CBC mode.
};
CHECK_TYPE(EncryptionScheme, 4, 4);

// The pattern used for pattern encryption. Note that ISO/IEC 23001-7:2016
// defines each block to be 16-bytes.
struct Pattern {
  uint32_t crypt_byte_block;  // Count of the encrypted blocks.
  uint32_t skip_byte_block;   // Count of the unencrypted blocks.
};
CHECK_TYPE(Pattern, 8, 8);

enum class ColorRange : uint8_t {
  kInvalid,
  kLimited,  // 709 color range with RGB values ranging from 16 to 235.
  kFull,     // Full RGB color range with RGB values from 0 to 255.
  kDerived   // Range is defined by |transfer_id| and |matrix_id|.
};
CHECK_TYPE(ColorRange, 1, 1);

// Described in ISO 23001-8:2016, section 7. All the IDs are in the range
// [0, 255] so 8-bit integer is sufficient. An unspecified ColorSpace should be
// {2, 2, 2, ColorRange::kInvalid}, where value 2 means "Unspecified" for all
// the IDs, as defined by the spec.
struct ColorSpace {
  uint8_t primary_id;   // 7.1 colour primaries, table 2
  uint8_t transfer_id;  // 7.2 transfer characteristics, table 3
  uint8_t matrix_id;    // 7.3 matrix coefficients, table 4
  ColorRange range;
};
CHECK_TYPE(ColorSpace, 4, 4);

// Time is defined as the number of seconds since the Epoch
// (00:00:00 UTC, January 1, 1970), not including any added leap second.
// Also see Time definition in spec: https://w3c.github.io/encrypted-media/#time
// Note that Time is defined in millisecond accuracy in the spec but in second
// accuracy here.
typedef double Time;

// An input buffer can be split into several continuous subsamples.
// A SubsampleEntry specifies the number of clear and cipher bytes in each
// subsample. For example, the following buffer has three subsamples:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
//
// For decryption, all of the cipher bytes in a buffer should be concatenated
// (in the subsample order) into a single logical stream. The clear bytes should
// not be considered as part of decryption.
//
// Stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
// Decrypted stream:    | decrypted1|  decrypted2 |   decrypted3  |
//
// After decryption, the decrypted bytes should be copied over the position
// of the corresponding cipher bytes in the original buffer to form the output
// buffer. Following the above example, the decrypted buffer should be:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
//
struct SubsampleEntry {
  uint32_t clear_bytes;
  uint32_t cipher_bytes;
};
CHECK_TYPE(SubsampleEntry, 8, 8);

// Represents an input buffer to be decrypted (and possibly decoded). It does
// not own any pointers in this struct. If |encryption_scheme| = kUnencrypted,
// the data is unencrypted.
// Note that this struct is organized so that sizeof(InputBuffer_2)
// equals the sum of sizeof() all members in both 32-bit and 64-bit compiles.
// Padding has been added to keep the fields aligned.
struct InputBuffer_2 {
  const uint8_t* data;  // Pointer to the beginning of the input data.
  uint32_t data_size;   // Size (in bytes) of |data|.

  EncryptionScheme encryption_scheme;

  const uint8_t* key_id;  // Key ID to identify the decryption key.
  uint32_t key_id_size;   // Size (in bytes) of |key_id|.
  uint32_t : 32;          // Padding.

  const uint8_t* iv;  // Initialization vector.
  uint32_t iv_size;   // Size (in bytes) of |iv|.
  uint32_t : 32;      // Padding.

  const struct SubsampleEntry* subsamples;
  uint32_t num_subsamples;  // Number of subsamples in |subsamples|.
  uint32_t : 32;            // Padding.

  // |pattern| is required if |encryption_scheme| specifies pattern encryption.
  Pattern pattern;

  int64_t timestamp;  // Presentation timestamp in microseconds.
};
CHECK_TYPE(InputBuffer_2, 64, 80);

enum AudioCodec : uint32_t { kUnknownAudioCodec = 0, kCodecVorbis, kCodecAac };
CHECK_TYPE(AudioCodec, 4, 4);

struct AudioDecoderConfig_2 {
  AudioCodec codec;
  int32_t channel_count;
  int32_t bits_per_channel;
  int32_t samples_per_second;

  // Optional byte data required to initialize audio decoders, such as the
  // vorbis setup header.
  uint8_t* extra_data;
  uint32_t extra_data_size;

  // Encryption scheme.
  EncryptionScheme encryption_scheme;
};
CHECK_TYPE(AudioDecoderConfig_2, 28, 32);

// Supported sample formats for AudioFrames.
enum AudioFormat : uint32_t {
  kUnknownAudioFormat = 0,  // Unknown format value. Used for error reporting.
  kAudioFormatU8,           // Interleaved unsigned 8-bit w/ bias of 128.
  kAudioFormatS16,          // Interleaved signed 16-bit.
  kAudioFormatS32,          // Interleaved signed 32-bit.
  kAudioFormatF32,          // Interleaved float 32-bit.
  kAudioFormatPlanarS16,    // Signed 16-bit planar.
  kAudioFormatPlanarF32,    // Float 32-bit planar.
};
CHECK_TYPE(AudioFormat, 4, 4);

// Surface formats based on FOURCC labels, see: http://www.fourcc.org/yuv.php
// Values are chosen to be consistent with Chromium's VideoPixelFormat values.
enum VideoFormat : uint32_t {
  kUnknownVideoFormat = 0,  // Unknown format value. Used for error reporting.
  kYv12 = 1,                // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  kI420 = 2,                // 12bpp YUV planar 1x1 Y, 2x2 UV samples.

  // In the following formats, each sample uses 16-bit in storage, while the
  // sample value is stored in the least significant N bits where N is
  // specified by the number after "P". For example, for YUV420P9, each Y, U,
  // and V sample is stored in the least significant 9 bits in a 2-byte block.
  kYUV420P9 = 16,
  kYUV420P10 = 17,
  kYUV422P9 = 18,
  kYUV422P10 = 19,
  kYUV444P9 = 20,
  kYUV444P10 = 21,
  kYUV420P12 = 22,
  kYUV422P12 = 23,
  kYUV444P12 = 24,
};
CHECK_TYPE(VideoFormat, 4, 4);

struct Size {
  int32_t width;
  int32_t height;
};
CHECK_TYPE(Size, 8, 8);

enum VideoCodec : uint32_t {
  kUnknownVideoCodec = 0,
  kCodecVp8,
  kCodecH264,
  kCodecVp9,
  kCodecAv1
};
CHECK_TYPE(VideoCodec, 4, 4);

enum VideoCodecProfile : uint32_t {
  kUnknownVideoCodecProfile = 0,
  kProfileNotNeeded,
  kH264ProfileBaseline,
  kH264ProfileMain,
  kH264ProfileExtended,
  kH264ProfileHigh,
  kH264ProfileHigh10,
  kH264ProfileHigh422,
  kH264ProfileHigh444Predictive,
  kVP9Profile0,
  kVP9Profile1,
  kVP9Profile2,
  kVP9Profile3,
  kAv1ProfileMain,
  kAv1ProfileHigh,
  kAv1ProfilePro
};
CHECK_TYPE(VideoCodecProfile, 4, 4);

// Deprecated: New CDM implementations should use VideoDecoderConfig_3.
// Note that this struct is organized so that sizeof(VideoDecoderConfig_2)
// equals the sum of sizeof() all members in both 32-bit and 64-bit compiles.
// Padding has been added to keep the fields aligned.
struct VideoDecoderConfig_2 {
  VideoCodec codec;
  VideoCodecProfile profile;
  VideoFormat format;
  uint32_t : 32;  // Padding.

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  Size coded_size;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8_t* extra_data;
  uint32_t extra_data_size;

  // Encryption scheme.
  EncryptionScheme encryption_scheme;
};
CHECK_TYPE(VideoDecoderConfig_2, 36, 40);

struct VideoDecoderConfig_3 {
  VideoCodec codec;
  VideoCodecProfile profile;
  VideoFormat format;
  ColorSpace color_space;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  Size coded_size;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8_t* extra_data;
  uint32_t extra_data_size;

  EncryptionScheme encryption_scheme;
};
CHECK_TYPE(VideoDecoderConfig_3, 36, 40);

enum StreamType : uint32_t { kStreamTypeAudio = 0, kStreamTypeVideo = 1 };
CHECK_TYPE(StreamType, 4, 4);

// Structure provided to ContentDecryptionModule::OnPlatformChallengeResponse()
// after a platform challenge was initiated via Host::SendPlatformChallenge().
// All values will be NULL / zero in the event of a challenge failure.
struct PlatformChallengeResponse {
  // |challenge| provided during Host::SendPlatformChallenge() combined with
  // nonce data and signed with the platform's private key.
  const uint8_t* signed_data;
  uint32_t signed_data_length;

  // RSASSA-PKCS1-v1_5-SHA256 signature of the |signed_data| block.
  const uint8_t* signed_data_signature;
  uint32_t signed_data_signature_length;

  // X.509 device specific certificate for the |service_id| requested.
  const uint8_t* platform_key_certificate;
  uint32_t platform_key_certificate_length;
};
CHECK_TYPE(PlatformChallengeResponse, 24, 48);

// The current status of the associated key. The valid types are defined in the
// spec: https://w3c.github.io/encrypted-media/#dom-mediakeystatus
enum KeyStatus : uint32_t {
  kUsable = 0,
  kInternalError = 1,
  kExpired = 2,
  kOutputRestricted = 3,
  kOutputDownscaled = 4,
  kStatusPending = 5,
  kReleased = 6
};
CHECK_TYPE(KeyStatus, 4, 4);

// Used when passing arrays of key information. Does not own the referenced
// data. |system_code| is an additional error code for unusable keys and
// should be 0 when |status| == kUsable.
struct KeyInformation {
  const uint8_t* key_id;
  uint32_t key_id_size;
  KeyStatus status;
  uint32_t system_code;
};
CHECK_TYPE(KeyInformation, 16, 24);

// Supported output protection methods for use with EnableOutputProtection() and
// returned by OnQueryOutputProtectionStatus().
enum OutputProtectionMethods : uint32_t {
  kProtectionNone = 0,
  kProtectionHDCP = 1 << 0
};
CHECK_TYPE(OutputProtectionMethods, 4, 4);

// Connected output link types returned by OnQueryOutputProtectionStatus().
enum OutputLinkTypes : uint32_t {
  kLinkTypeNone = 0,
  kLinkTypeUnknown = 1 << 0,
  kLinkTypeInternal = 1 << 1,
  kLinkTypeVGA = 1 << 2,
  kLinkTypeHDMI = 1 << 3,
  kLinkTypeDVI = 1 << 4,
  kLinkTypeDisplayPort = 1 << 5,
  kLinkTypeNetwork = 1 << 6
};
CHECK_TYPE(OutputLinkTypes, 4, 4);

// Result of the QueryOutputProtectionStatus() call.
enum QueryResult : uint32_t { kQuerySucceeded = 0, kQueryFailed };
CHECK_TYPE(QueryResult, 4, 4);

// The Initialization Data Type. The valid types are defined in the spec:
// https://w3c.github.io/encrypted-media/format-registry/initdata/index.html#registry
enum InitDataType : uint32_t { kCenc = 0, kKeyIds = 1, kWebM = 2 };
CHECK_TYPE(InitDataType, 4, 4);

// The type of session to create. The valid types are defined in the spec:
// https://w3c.github.io/encrypted-media/#dom-mediakeysessiontype
enum SessionType : uint32_t {
  kTemporary = 0,
  kPersistentLicense = 1,
  kPersistentUsageRecord = 2
};
CHECK_TYPE(SessionType, 4, 4);

// The type of the message event.  The valid types are defined in the spec:
// https://w3c.github.io/encrypted-media/#dom-mediakeymessagetype
enum MessageType : uint32_t {
  kLicenseRequest = 0,
  kLicenseRenewal = 1,
  kLicenseRelease = 2,
  kIndividualizationRequest = 3
};
CHECK_TYPE(MessageType, 4, 4);

enum HdcpVersion : uint32_t {
  kHdcpVersionNone,
  kHdcpVersion1_0,
  kHdcpVersion1_1,
  kHdcpVersion1_2,
  kHdcpVersion1_3,
  kHdcpVersion1_4,
  kHdcpVersion2_0,
  kHdcpVersion2_1,
  kHdcpVersion2_2,
  kHdcpVersion2_3
};
CHECK_TYPE(HdcpVersion, 4, 4);

struct Policy {
  HdcpVersion min_hdcp_version;
};
CHECK_TYPE(Policy, 4, 4);

// Represents a buffer created by Allocator implementations.
class CDM_CLASS_API Buffer {
 public:
  // Destroys the buffer in the same context as it was created.
  virtual void Destroy() = 0;

  virtual uint32_t Capacity() const = 0;
  virtual uint8_t* Data() = 0;
  virtual void SetSize(uint32_t size) = 0;
  virtual uint32_t Size() const = 0;

 protected:
  Buffer() {}
  virtual ~Buffer() {}

 private:
  Buffer(const Buffer&);
  void operator=(const Buffer&);
};

// Represents a decrypted block that has not been decoded.
class CDM_CLASS_API DecryptedBlock {
 public:
  virtual void SetDecryptedBuffer(Buffer* buffer) = 0;
  virtual Buffer* DecryptedBuffer() = 0;

  // TODO(tomfinegan): Figure out if timestamp is really needed. If it is not,
  // we can just pass Buffer pointers around.
  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  DecryptedBlock() {}
  virtual ~DecryptedBlock() {}
};

enum VideoPlane : uint32_t {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kMaxPlanes = 3,
};
CHECK_TYPE(VideoPlane, 4, 4);

class CDM_CLASS_API VideoFrame {
 public:
  virtual void SetFormat(VideoFormat format) = 0;
  virtual VideoFormat Format() const = 0;

  virtual void SetSize(cdm::Size size) = 0;
  virtual cdm::Size Size() const = 0;

  virtual void SetFrameBuffer(Buffer* frame_buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

  virtual void SetPlaneOffset(VideoPlane plane, uint32_t offset) = 0;
  virtual uint32_t PlaneOffset(VideoPlane plane) = 0;

  virtual void SetStride(VideoPlane plane, uint32_t stride) = 0;
  virtual uint32_t Stride(VideoPlane plane) = 0;

  // Sets and gets the presentation timestamp which is in microseconds.
  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  VideoFrame() {}
  virtual ~VideoFrame() {}
};

// Represents a decoded video frame. The CDM should call the interface methods
// to set the frame attributes. See DecryptAndDecodeFrame().
class CDM_CLASS_API VideoFrame_2 {
 public:
  virtual void SetFormat(VideoFormat format) = 0;
  virtual void SetSize(cdm::Size size) = 0;
  virtual void SetFrameBuffer(Buffer* frame_buffer) = 0;
  virtual void SetPlaneOffset(VideoPlane plane, uint32_t offset) = 0;
  virtual void SetStride(VideoPlane plane, uint32_t stride) = 0;
  // Sets the presentation timestamp which is in microseconds.
  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual void SetColorSpace(ColorSpace color_space) = 0;

 protected:
  VideoFrame_2() {}
  virtual ~VideoFrame_2() {}
};

// Represents decrypted and decoded audio frames. AudioFrames can contain
// multiple audio output buffers, which are serialized into this format:
//
// |<------------------- serialized audio buffer ------------------->|
// | int64_t timestamp | int64_t length | length bytes of audio data |
//
// For example, with three audio output buffers, the AudioFrames will look
// like this:
//
// |<----------------- AudioFrames ------------------>|
// | audio buffer 0 | audio buffer 1 | audio buffer 2 |
class CDM_CLASS_API AudioFrames {
 public:
  virtual void SetFrameBuffer(Buffer* buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

  // The CDM must call this method, providing a valid format, when providing
  // frame buffers. Planar data should be stored end to end; e.g.,
  // |ch1 sample1||ch1 sample2|....|ch1 sample_last||ch2 sample1|...
  virtual void SetFormat(AudioFormat format) = 0;
  virtual AudioFormat Format() const = 0;

 protected:
  AudioFrames() {}
  virtual ~AudioFrames() {}
};

// FileIO interface provides a way for the CDM to store data in a file in
// persistent storage. This interface aims only at providing basic read/write
// capabilities and should not be used as a full fledged file IO API.
// Each CDM and origin (e.g. HTTPS, "foo.example.com", 443) combination has
// its own persistent storage. All instances of a given CDM associated with a
// given origin share the same persistent storage.
// Note to implementors of this interface:
// Per-origin storage and the ability for users to clear it are important.
// See http://www.w3.org/TR/encrypted-media/#privacy-storedinfo.
class CDM_CLASS_API FileIO {
 public:
  // Opens the file with |file_name| for read and write.
  // FileIOClient::OnOpenComplete() will be called after the opening
  // operation finishes.
  // - When the file is opened by a CDM instance, it will be classified as "in
  //   use". In this case other CDM instances in the same domain may receive
  //   kInUse status when trying to open it.
  // - |file_name| must only contain letters (A-Za-z), digits(0-9), or "._-".
  //   It must not start with an underscore ('_'), and must be at least 1
  //   character and no more than 256 characters long.
  virtual void Open(const char* file_name, uint32_t file_name_size) = 0;

  // Reads the contents of the file. FileIOClient::OnReadComplete() will be
  // called with the read status. Read() should not be called if a previous
  // Read() or Write() call is still pending; otherwise OnReadComplete() will
  // be called with kInUse.
  virtual void Read() = 0;

  // Writes |data_size| bytes of |data| into the file.
  // FileIOClient::OnWriteComplete() will be called with the write status.
  // All existing contents in the file will be overwritten. Calling Write() with
  // NULL |data| will clear all contents in the file. Write() should not be
  // called if a previous Write() or Read() call is still pending; otherwise
  // OnWriteComplete() will be called with kInUse.
  virtual void Write(const uint8_t* data, uint32_t data_size) = 0;

  // Closes the file if opened, destroys this FileIO object and releases any
  // resources allocated. The CDM must call this method when it finished using
  // this object. A FileIO object must not be used after Close() is called.
  virtual void Close() = 0;

 protected:
  FileIO() {}
  virtual ~FileIO() {}
};

// Responses to FileIO calls. All responses will be called asynchronously.
// When kError is returned, the FileIO object could be in an error state. All
// following calls (other than Close()) could return kError. The CDM should
// still call Close() to destroy the FileIO object.
class CDM_CLASS_API FileIOClient {
 public:
  enum class Status : uint32_t { kSuccess = 0, kInUse, kError };

  // Response to a FileIO::Open() call with the open |status|.
  virtual void OnOpenComplete(Status status) = 0;

  // Response to a FileIO::Read() call to provide |data_size| bytes of |data|
  // read from the file.
  // - kSuccess indicates that all contents of the file has been successfully
  //   read. In this case, 0 |data_size| means that the file is empty.
  // - kInUse indicates that there are other read/write operations pending.
  // - kError indicates read failure, e.g. the storage is not open or cannot be
  //   fully read.
  virtual void OnReadComplete(Status status, const uint8_t* data,
                              uint32_t data_size) = 0;

  // Response to a FileIO::Write() call.
  // - kSuccess indicates that all the data has been written into the file
  //   successfully.
  // - kInUse indicates that there are other read/write operations pending.
  // - kError indicates write failure, e.g. the storage is not open or cannot be
  //   fully written. Upon write failure, the contents of the file should be
  //   regarded as corrupt and should not used.
  virtual void OnWriteComplete(Status status) = 0;

 protected:
  FileIOClient() {}
  virtual ~FileIOClient() {}
};

class CDM_CLASS_API Host_10;
class CDM_CLASS_API Host_11;

// ContentDecryptionModule interface that all CDMs need to implement.
// The interface is versioned for backward compatibility.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class CDM_CLASS_API ContentDecryptionModule_10 {
 public:
  static const int kVersion = 10;
  static const bool kIsStable = true;
  typedef Host_10 Host;

  // Initializes the CDM instance, providing information about permitted
  // functionalities. The CDM must respond by calling Host::OnInitialized()
  // with whether the initialization succeeded. No other calls will be made by
  // the host before Host::OnInitialized() returns.
  // If |allow_distinctive_identifier| is false, messages from the CDM,
  // such as message events, must not contain a Distinctive Identifier,
  // even in an encrypted form.
  // If |allow_persistent_state| is false, the CDM must not attempt to
  // persist state. Calls to CreateFileIO() will fail.
  // If |use_hw_secure_codecs| is true, the CDM must ensure the decryption key
  // and video buffers (compressed and uncompressed) are securely protected by
  // hardware.
  virtual void Initialize(bool allow_distinctive_identifier,
                          bool allow_persistent_state,
                          bool use_hw_secure_codecs) = 0;

  // Gets the key status if the CDM has a hypothetical key with the |policy|.
  // The CDM must respond by calling either Host::OnResolveKeyStatusPromise()
  // with the result key status or Host::OnRejectPromise() if an unexpected
  // error happened or this method is not supported.
  virtual void GetStatusForPolicy(uint32_t promise_id,
                                  const Policy& policy) = 0;

  // SetServerCertificate(), CreateSessionAndGenerateRequest(), LoadSession(),
  // UpdateSession(), CloseSession(), and RemoveSession() all accept a
  // |promise_id|, which must be passed to the completion Host method
  // (e.g. Host::OnResolveNewSessionPromise()).

  // Provides a server certificate to be used to encrypt messages to the
  // license server. The CDM must respond by calling either
  // Host::OnResolvePromise() or Host::OnRejectPromise().
  // If the CDM does not support server certificates, the promise should be
  // rejected with kExceptionNotSupportedError. If |server_certificate_data|
  // is empty, reject with kExceptionTypeError. Any other error should be
  // rejected with kExceptionInvalidStateError or kExceptionQuotaExceededError.
  // TODO(crbug.com/796417): Add support for the promise to return true or
  // false, rather than using kExceptionNotSupportedError to mean false.
  virtual void SetServerCertificate(uint32_t promise_id,
                                    const uint8_t* server_certificate_data,
                                    uint32_t server_certificate_data_size) = 0;

  // Creates a session given |session_type|, |init_data_type|, and |init_data|.
  // The CDM must respond by calling either Host::OnResolveNewSessionPromise()
  // or Host::OnRejectPromise().
  virtual void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                               SessionType session_type,
                                               InitDataType init_data_type,
                                               const uint8_t* init_data,
                                               uint32_t init_data_size) = 0;

  // Loads the session of type |session_type| specified by |session_id|.
  // The CDM must respond by calling either Host::OnResolveNewSessionPromise()
  // or Host::OnRejectPromise(). If the session is not found, call
  // Host::OnResolveNewSessionPromise() with session_id = NULL.
  virtual void LoadSession(uint32_t promise_id, SessionType session_type,
                           const char* session_id,
                           uint32_t session_id_size) = 0;

  // Updates the session with |response|. The CDM must respond by calling
  // either Host::OnResolvePromise() or Host::OnRejectPromise().
  virtual void UpdateSession(uint32_t promise_id, const char* session_id,
                             uint32_t session_id_size, const uint8_t* response,
                             uint32_t response_size) = 0;

  // Requests that the CDM close the session. The CDM must respond by calling
  // either Host::OnResolvePromise() or Host::OnRejectPromise() when the request
  // has been processed. This may be before the session is closed. Once the
  // session is closed, Host::OnSessionClosed() must also be called.
  virtual void CloseSession(uint32_t promise_id, const char* session_id,
                            uint32_t session_id_size) = 0;

  // Removes any stored session data associated with this session. Will only be
  // called for persistent sessions. The CDM must respond by calling either
  // Host::OnResolvePromise() or Host::OnRejectPromise() when the request has
  // been processed.
  virtual void RemoveSession(uint32_t promise_id, const char* session_id,
                             uint32_t session_id_size) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer_2& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kInitializationError if |audio_decoder_config| is not supported.
  // The CDM may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig_2& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kInitializationError if |video_decoder_config| is not supported.
  // The CDM may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig_2& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until
  // kNeedMoreData is returned.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer_2& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer_2& encrypted_buffer,
                                         AudioFrames* audio_frames) = 0;

  // Called by the host after a platform challenge was initiated via
  // Host::SendPlatformChallenge().
  virtual void OnPlatformChallengeResponse(
      const PlatformChallengeResponse& response) = 0;

  // Called by the host after a call to Host::QueryOutputProtectionStatus(). The
  // |link_mask| is a bit mask of OutputLinkTypes and |output_protection_mask|
  // is a bit mask of OutputProtectionMethods. If |result| is kQueryFailed,
  // then |link_mask| and |output_protection_mask| are undefined and should
  // be ignored.
  virtual void OnQueryOutputProtectionStatus(
      QueryResult result, uint32_t link_mask,
      uint32_t output_protection_mask) = 0;

  // Called by the host after a call to Host::RequestStorageId(). If the
  // version of the storage ID requested is available, |storage_id| and
  // |storage_id_size| are set appropriately. |version| will be the same as
  // what was requested, unless 0 (latest) was requested, in which case
  // |version| will be the actual version number for the |storage_id| returned.
  // If the requested version is not available, null/zero will be provided as
  // |storage_id| and |storage_id_size|, respectively, and |version| should be
  // ignored.
  virtual void OnStorageId(uint32_t version, const uint8_t* storage_id,
                           uint32_t storage_id_size) = 0;

  // Destroys the object in the same context as it was created.
  virtual void Destroy() = 0;

 protected:
  ContentDecryptionModule_10() {}
  virtual ~ContentDecryptionModule_10() {}
};

// ----- Note: CDM interface(s) below still in development and not stable! -----

// ContentDecryptionModule interface that all CDMs need to implement.
// The interface is versioned for backward compatibility.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class CDM_CLASS_API ContentDecryptionModule_11 {
 public:
  static const int kVersion = 11;
  static const bool kIsStable = false;
  typedef Host_11 Host;

  // Initializes the CDM instance, providing information about permitted
  // functionalities. The CDM must respond by calling Host::OnInitialized()
  // with whether the initialization succeeded. No other calls will be made by
  // the host before Host::OnInitialized() returns.
  // If |allow_distinctive_identifier| is false, messages from the CDM,
  // such as message events, must not contain a Distinctive Identifier,
  // even in an encrypted form.
  // If |allow_persistent_state| is false, the CDM must not attempt to
  // persist state. Calls to CreateFileIO() will fail.
  // If |use_hw_secure_codecs| is true, the CDM must ensure the decryption key
  // and video buffers (compressed and uncompressed) are securely protected by
  // hardware.
  virtual void Initialize(bool allow_distinctive_identifier,
                          bool allow_persistent_state,
                          bool use_hw_secure_codecs) = 0;

  // Gets the key status if the CDM has a hypothetical key with the |policy|.
  // The CDM must respond by calling either Host::OnResolveKeyStatusPromise()
  // with the result key status or Host::OnRejectPromise() if an unexpected
  // error happened or this method is not supported.
  virtual void GetStatusForPolicy(uint32_t promise_id,
                                  const Policy& policy) = 0;

  // SetServerCertificate(), CreateSessionAndGenerateRequest(), LoadSession(),
  // UpdateSession(), CloseSession(), and RemoveSession() all accept a
  // |promise_id|, which must be passed to the completion Host method
  // (e.g. Host::OnResolveNewSessionPromise()).

  // Provides a server certificate to be used to encrypt messages to the
  // license server. The CDM must respond by calling either
  // Host::OnResolvePromise() or Host::OnRejectPromise().
  // If the CDM does not support server certificates, the promise should be
  // rejected with kExceptionNotSupportedError. If |server_certificate_data|
  // is empty, reject with kExceptionTypeError. Any other error should be
  // rejected with kExceptionInvalidStateError or kExceptionQuotaExceededError.
  // TODO(crbug.com/796417): Add support for the promise to return true or
  // false, rather than using kExceptionNotSupportedError to mean false.
  virtual void SetServerCertificate(uint32_t promise_id,
                                    const uint8_t* server_certificate_data,
                                    uint32_t server_certificate_data_size) = 0;

  // Creates a session given |session_type|, |init_data_type|, and |init_data|.
  // The CDM must respond by calling either Host::OnResolveNewSessionPromise()
  // or Host::OnRejectPromise().
  virtual void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                               SessionType session_type,
                                               InitDataType init_data_type,
                                               const uint8_t* init_data,
                                               uint32_t init_data_size) = 0;

  // Loads the session of type |session_type| specified by |session_id|.
  // The CDM must respond by calling either Host::OnResolveNewSessionPromise()
  // or Host::OnRejectPromise(). If the session is not found, call
  // Host::OnResolveNewSessionPromise() with session_id = NULL.
  virtual void LoadSession(uint32_t promise_id, SessionType session_type,
                           const char* session_id,
                           uint32_t session_id_size) = 0;

  // Updates the session with |response|. The CDM must respond by calling
  // either Host::OnResolvePromise() or Host::OnRejectPromise().
  virtual void UpdateSession(uint32_t promise_id, const char* session_id,
                             uint32_t session_id_size, const uint8_t* response,
                             uint32_t response_size) = 0;

  // Requests that the CDM close the session. The CDM must respond by calling
  // either Host::OnResolvePromise() or Host::OnRejectPromise() when the request
  // has been processed. This may be before the session is closed. Once the
  // session is closed, Host::OnSessionClosed() must also be called.
  virtual void CloseSession(uint32_t promise_id, const char* session_id,
                            uint32_t session_id_size) = 0;

  // Removes any stored session data associated with this session. Removes all
  // license(s) and key(s) associated with the session, whether they are in
  // memory, persistent store, or both. For persistent session types, other
  // session data (e.g. record of license destruction) will be cleared as
  // defined for each session type once a release message acknowledgment is
  // processed by UpdateSession(). The CDM must respond by calling either
  // Host::OnResolvePromise() or Host::OnRejectPromise() when the request has
  // been processed.
  virtual void RemoveSession(uint32_t promise_id, const char* session_id,
                             uint32_t session_id_size) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer_2& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kInitializationError if |audio_decoder_config| is not supported.
  // The CDM may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig_2& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kInitializationError if |video_decoder_config| is not supported.
  // The CDM may still be able to do Decrypt().
  // Returns kDeferredInitialization if the CDM is not ready to initialize the
  // decoder at this time. Must call Host::OnDeferredInitializationDone() once
  // initialization is complete.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig_3& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until
  // kNeedMoreData is returned.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer_2& encrypted_buffer,
                                       VideoFrame_2* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer_2& encrypted_buffer,
                                         AudioFrames* audio_frames) = 0;

  // Called by the host after a platform challenge was initiated via
  // Host::SendPlatformChallenge().
  virtual void OnPlatformChallengeResponse(
      const PlatformChallengeResponse& response) = 0;

  // Called by the host after a call to Host::QueryOutputProtectionStatus(). The
  // |link_mask| is a bit mask of OutputLinkTypes and |output_protection_mask|
  // is a bit mask of OutputProtectionMethods. If |result| is kQueryFailed,
  // then |link_mask| and |output_protection_mask| are undefined and should
  // be ignored.
  virtual void OnQueryOutputProtectionStatus(
      QueryResult result, uint32_t link_mask,
      uint32_t output_protection_mask) = 0;

  // Called by the host after a call to Host::RequestStorageId(). If the
  // version of the storage ID requested is available, |storage_id| and
  // |storage_id_size| are set appropriately. |version| will be the same as
  // what was requested, unless 0 (latest) was requested, in which case
  // |version| will be the actual version number for the |storage_id| returned.
  // If the requested version is not available, null/zero will be provided as
  // |storage_id| and |storage_id_size|, respectively, and |version| should be
  // ignored.
  virtual void OnStorageId(uint32_t version, const uint8_t* storage_id,
                           uint32_t storage_id_size) = 0;

  // Destroys the object in the same context as it was created.
  virtual void Destroy() = 0;

 protected:
  ContentDecryptionModule_11() {}
  virtual ~ContentDecryptionModule_11() {}
};

class CDM_CLASS_API Host_10 {
 public:
  static const int kVersion = 10;

  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(uint32_t capacity) = 0;

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current wall time.
  virtual Time GetCurrentWallTime() = 0;

  // Called by the CDM with the result after the CDM instance was initialized.
  virtual void OnInitialized(bool success) = 0;

  // Called by the CDM when a key status is available in response to
  // GetStatusForPolicy().
  virtual void OnResolveKeyStatusPromise(uint32_t promise_id,
                                         KeyStatus key_status) = 0;

  // Called by the CDM when a session is created or loaded and the value for the
  // MediaKeySession's sessionId attribute is available (|session_id|).
  // This must be called before OnSessionMessage() or
  // OnSessionKeysChange() is called for the same session. |session_id_size|
  // should not include null termination.
  // When called in response to LoadSession(), the |session_id| must be the
  // same as the |session_id| passed in LoadSession(), or NULL if the
  // session could not be loaded.
  virtual void OnResolveNewSessionPromise(uint32_t promise_id,
                                          const char* session_id,
                                          uint32_t session_id_size) = 0;

  // Called by the CDM when a session is updated or released.
  virtual void OnResolvePromise(uint32_t promise_id) = 0;

  // Called by the CDM when an error occurs as a result of one of the
  // ContentDecryptionModule calls that accept a |promise_id|.
  // |exception| must be specified. |error_message| and |system_code|
  // are optional. |error_message_size| should not include null termination.
  virtual void OnRejectPromise(uint32_t promise_id, Exception exception,
                               uint32_t system_code, const char* error_message,
                               uint32_t error_message_size) = 0;

  // Called by the CDM when it has a message for session |session_id|.
  // Size parameters should not include null termination.
  virtual void OnSessionMessage(const char* session_id,
                                uint32_t session_id_size,
                                MessageType message_type, const char* message,
                                uint32_t message_size) = 0;

  // Called by the CDM when there has been a change in keys or their status for
  // session |session_id|. |has_additional_usable_key| should be set if a
  // key is newly usable (e.g. new key available, previously expired key has
  // been renewed, etc.) and the browser should attempt to resume playback.
  // |keys_info| is the list of key IDs for this session along with their
  // current status. |keys_info_count| is the number of entries in |keys_info|.
  // Size parameter for |session_id| should not include null termination.
  virtual void OnSessionKeysChange(const char* session_id,
                                   uint32_t session_id_size,
                                   bool has_additional_usable_key,
                                   const KeyInformation* keys_info,
                                   uint32_t keys_info_count) = 0;

  // Called by the CDM when there has been a change in the expiration time for
  // session |session_id|. This can happen as the result of an Update() call
  // or some other event. If this happens as a result of a call to Update(),
  // it must be called before resolving the Update() promise. |new_expiry_time|
  // represents the time after which the key(s) in the session will no longer
  // be usable for decryption. It can be 0 if no such time exists or if the
  // license explicitly never expires. Size parameter should not include null
  // termination.
  virtual void OnExpirationChange(const char* session_id,
                                  uint32_t session_id_size,
                                  Time new_expiry_time) = 0;

  // Called by the CDM when session |session_id| is closed. Size
  // parameter should not include null termination.
  virtual void OnSessionClosed(const char* session_id,
                               uint32_t session_id_size) = 0;

  // The following are optional methods that may not be implemented on all
  // platforms.

  // Sends a platform challenge for the given |service_id|. |challenge| is at
  // most 256 bits of data to be signed. Once the challenge has been completed,
  // the host will call ContentDecryptionModule::OnPlatformChallengeResponse()
  // with the signed challenge response and platform certificate. Size
  // parameters should not include null termination.
  virtual void SendPlatformChallenge(const char* service_id,
                                     uint32_t service_id_size,
                                     const char* challenge,
                                     uint32_t challenge_size) = 0;

  // Attempts to enable output protection (e.g. HDCP) on the display link. The
  // |desired_protection_mask| is a bit mask of OutputProtectionMethods. No
  // status callback is issued, the CDM must call QueryOutputProtectionStatus()
  // periodically to ensure the desired protections are applied.
  virtual void EnableOutputProtection(uint32_t desired_protection_mask) = 0;

  // Requests the current output protection status. Once the host has the status
  // it will call ContentDecryptionModule::OnQueryOutputProtectionStatus().
  virtual void QueryOutputProtectionStatus() = 0;

  // Must be called by the CDM if it returned kDeferredInitialization during
  // InitializeAudioDecoder() or InitializeVideoDecoder().
  virtual void OnDeferredInitializationDone(StreamType stream_type,
                                            Status decoder_status) = 0;

  // Creates a FileIO object from the host to do file IO operation. Returns NULL
  // if a FileIO object cannot be obtained. Once a valid FileIO object is
  // returned, |client| must be valid until FileIO::Close() is called. The
  // CDM can call this method multiple times to operate on different files.
  virtual FileIO* CreateFileIO(FileIOClient* client) = 0;

  // Requests a specific version of the storage ID. A storage ID is a stable,
  // device specific ID used by the CDM to securely store persistent data. The
  // ID will be returned by the host via ContentDecryptionModule::OnStorageId().
  // If |version| is 0, the latest version will be returned. All |version|s
  // that are greater than or equal to 0x80000000 are reserved for the CDM and
  // should not be supported or returned by the host. The CDM must not expose
  // the ID outside the client device, even in encrypted form.
  virtual void RequestStorageId(uint32_t version) = 0;

 protected:
  Host_10() {}
  virtual ~Host_10() {}
};

class CDM_CLASS_API Host_11 {
 public:
  static const int kVersion = 11;

  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(uint32_t capacity) = 0;

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current wall time.
  virtual Time GetCurrentWallTime() = 0;

  // Called by the CDM with the result after the CDM instance was initialized.
  virtual void OnInitialized(bool success) = 0;

  // Called by the CDM when a key status is available in response to
  // GetStatusForPolicy().
  virtual void OnResolveKeyStatusPromise(uint32_t promise_id,
                                         KeyStatus key_status) = 0;

  // Called by the CDM when a session is created or loaded and the value for the
  // MediaKeySession's sessionId attribute is available (|session_id|).
  // This must be called before OnSessionMessage() or
  // OnSessionKeysChange() is called for the same session. |session_id_size|
  // should not include null termination.
  // When called in response to LoadSession(), the |session_id| must be the
  // same as the |session_id| passed in LoadSession(), or NULL if the
  // session could not be loaded.
  virtual void OnResolveNewSessionPromise(uint32_t promise_id,
                                          const char* session_id,
                                          uint32_t session_id_size) = 0;

  // Called by the CDM when a session is updated or released.
  virtual void OnResolvePromise(uint32_t promise_id) = 0;

  // Called by the CDM when an error occurs as a result of one of the
  // ContentDecryptionModule calls that accept a |promise_id|.
  // |exception| must be specified. |error_message| and |system_code|
  // are optional. |error_message_size| should not include null termination.
  virtual void OnRejectPromise(uint32_t promise_id, Exception exception,
                               uint32_t system_code, const char* error_message,
                               uint32_t error_message_size) = 0;

  // Called by the CDM when it has a message for session |session_id|.
  // Size parameters should not include null termination.
  virtual void OnSessionMessage(const char* session_id,
                                uint32_t session_id_size,
                                MessageType message_type, const char* message,
                                uint32_t message_size) = 0;

  // Called by the CDM when there has been a change in keys or their status for
  // session |session_id|. |has_additional_usable_key| should be set if a
  // key is newly usable (e.g. new key available, previously expired key has
  // been renewed, etc.) and the browser should attempt to resume playback.
  // |keys_info| is the list of key IDs for this session along with their
  // current status. |keys_info_count| is the number of entries in |keys_info|.
  // Size parameter for |session_id| should not include null termination.
  virtual void OnSessionKeysChange(const char* session_id,
                                   uint32_t session_id_size,
                                   bool has_additional_usable_key,
                                   const KeyInformation* keys_info,
                                   uint32_t keys_info_count) = 0;

  // Called by the CDM when there has been a change in the expiration time for
  // session |session_id|. This can happen as the result of an Update() call
  // or some other event. If this happens as a result of a call to Update(),
  // it must be called before resolving the Update() promise. |new_expiry_time|
  // represents the time after which the key(s) in the session will no longer
  // be usable for decryption. It can be 0 if no such time exists or if the
  // license explicitly never expires. Size parameter should not include null
  // termination.
  virtual void OnExpirationChange(const char* session_id,
                                  uint32_t session_id_size,
                                  Time new_expiry_time) = 0;

  // Called by the CDM when session |session_id| is closed. Size
  // parameter should not include null termination.
  virtual void OnSessionClosed(const char* session_id,
                               uint32_t session_id_size) = 0;

  // The following are optional methods that may not be implemented on all
  // platforms.

  // Sends a platform challenge for the given |service_id|. |challenge| is at
  // most 256 bits of data to be signed. Once the challenge has been completed,
  // the host will call ContentDecryptionModule::OnPlatformChallengeResponse()
  // with the signed challenge response and platform certificate. Size
  // parameters should not include null termination.
  virtual void SendPlatformChallenge(const char* service_id,
                                     uint32_t service_id_size,
                                     const char* challenge,
                                     uint32_t challenge_size) = 0;

  // Attempts to enable output protection (e.g. HDCP) on the display link. The
  // |desired_protection_mask| is a bit mask of OutputProtectionMethods. No
  // status callback is issued, the CDM must call QueryOutputProtectionStatus()
  // periodically to ensure the desired protections are applied.
  virtual void EnableOutputProtection(uint32_t desired_protection_mask) = 0;

  // Requests the current output protection status. Once the host has the status
  // it will call ContentDecryptionModule::OnQueryOutputProtectionStatus().
  virtual void QueryOutputProtectionStatus() = 0;

  // Must be called by the CDM if it returned kDeferredInitialization during
  // InitializeAudioDecoder() or InitializeVideoDecoder().
  virtual void OnDeferredInitializationDone(StreamType stream_type,
                                            Status decoder_status) = 0;

  // Creates a FileIO object from the host to do file IO operation. Returns NULL
  // if a FileIO object cannot be obtained. Once a valid FileIO object is
  // returned, |client| must be valid until FileIO::Close() is called. The
  // CDM can call this method multiple times to operate on different files.
  virtual FileIO* CreateFileIO(FileIOClient* client) = 0;

  // Requests a CdmProxy that proxies part of CDM functionalities to a different
  // entity, e.g. a hardware CDM module. A CDM instance can have at most one
  // CdmProxy throughout its lifetime, which must be requested and initialized
  // during CDM instance initialization time, i.e. in or after CDM::Initialize()
  // and before OnInitialized() is called, to ensure proper connection of the
  // CdmProxy and the media player (e.g. hardware decoder). The CdmProxy is
  // owned by the host and is guaranteed to be valid throughout the CDM
  // instance's lifetime. The CDM must ensure that the |client| remain valid
  // before the CDM instance is destroyed. Returns null if CdmProxy is not
  // supported, called before CDM::Initialize(), RequestCdmProxy() is called
  // more than once, or called after the CDM instance has been initialized.
  virtual CdmProxy* RequestCdmProxy(CdmProxyClient* client) = 0;

  // Requests a specific version of the storage ID. A storage ID is a stable,
  // device specific ID used by the CDM to securely store persistent data. The
  // ID will be returned by the host via ContentDecryptionModule::OnStorageId().
  // If |version| is 0, the latest version will be returned. All |version|s
  // that are greater than or equal to 0x80000000 are reserved for the CDM and
  // should not be supported or returned by the host. The CDM must not expose
  // the ID outside the client device, even in encrypted form.
  virtual void RequestStorageId(uint32_t version) = 0;

 protected:
  Host_11() {}
  virtual ~Host_11() {}
};

}  // namespace cdm

#endif  // CDM_CONTENT_DECRYPTION_MODULE_H_
