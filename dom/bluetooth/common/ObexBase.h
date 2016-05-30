/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ObexBase_h
#define mozilla_dom_bluetooth_ObexBase_h

#include "BluetoothCommon.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

BEGIN_BLUETOOTH_NAMESPACE

const char FINAL_BIT = 0x80;

/**
 * Section 3.2 "Response format", IrOBEX ver 1.2
 * The format of an OBEX response header is
 * [response code:1][response length:2]
 */
static const uint32_t kObexRespHeaderSize = 3;

/**
 * Section 2.2.9 "Body, End-of-Body", IrOBEX ver 1.2
 * The format of an OBEX Body header is
 * [headerId:1][header length:2]
 */
static const uint32_t kObexBodyHeaderSize = 3;

/**
 * Section 3.3.1.4 "Minimum OBEX Packet Length", IrOBEX ver 1.2
 * The minimum size of the OBEX Maximum packet length allowed for negotiation is
 * 255 bytes.
 */
static const uint32_t kObexLeastMaxSize = 255;

/*
 * Defined in section 2.1 "OBEX Headers", IrOBEX ver 1.2
 */
enum ObexHeaderId {
  Count = 0xC0,
  Name = 0x01,
  Type = 0x42,
  Length = 0xC3,
  TimeISO8601 = 0x44,
  Time4Byte = 0xC4,
  Description = 0x05,
  Target = 0x46,
  HTTP = 0x47,
  Body = 0x48,
  EndOfBody = 0x49,
  Who = 0x4A,
  ConnectionId = 0xCB,
  AppParameters = 0x4C,
  AuthChallenge = 0x4D,
  AuthResponse = 0x4E,
  ObjectClass = 0x4F
};

/*
 * Defined in section 3.3 "OBEX Operations and Opcode definitions",
 * IrOBEX ver 1.2
 */
enum ObexRequestCode {
  Connect = 0x80,
  Disconnect = 0x81,
  Put = 0x02,
  PutFinal = 0x82,
  Get = 0x03,
  GetFinal = 0x83,
  SetPath = 0x85,
  Abort = 0xFF
};

/*
 * Defined in section 3.2.1 "Response Code values", IrOBEX ver 1.2
 */
enum ObexResponseCode {
  Continue = 0x90,

  Success = 0xA0,
  Created = 0xA1,
  Accepted = 0xA2,
  NonAuthoritativeInfo = 0xA3,
  NoContent = 0xA4,
  ResetContent = 0xA5,
  PartialContent = 0xA6,

  MultipleChoices = 0xB0,
  MovedPermanently = 0xB1,
  MovedTemporarily = 0xB2,
  SeeOther = 0xB3,
  NotModified = 0xB4,
  UseProxy = 0xB5,

  BadRequest = 0xC0,
  Unauthorized = 0xC1,
  PaymentRequired = 0xC2,
  Forbidden = 0xC3,
  NotFound = 0xC4,
  MethodNotAllowed = 0xC5,
  NotAcceptable = 0xC6,
  ProxyAuthenticationRequired = 0xC7,
  RequestTimeOut = 0xC8,
  Conflict = 0xC9,
  Gone = 0xCA,
  LengthRequired = 0xCB,
  PreconditionFailed = 0xCC,
  RequestedEntityTooLarge = 0xCD,
  RequestUrlTooLarge = 0xCE,
  UnsupprotedMediaType = 0xCF,

  InternalServerError = 0xD0,
  NotImplemented = 0xD1,
  BadGateway = 0xD2,
  ServiceUnavailable = 0xD3,
  GatewayTimeout = 0xD4,
  HttpVersionNotSupported = 0xD5,

  DatabaseFull = 0xE0,
  DatabaseLocked = 0xE1,
};

enum ObexDigestChallenge {
  Nonce = 0x00,
  Options = 0x01,
  Realm = 0x02
};

enum ObexDigestResponse {
  ReqDigest = 0x00,
  UserId = 0x01,
  NonceChallenged = 0x02
};

class ObexHeader
{
public:
  ObexHeader(ObexHeaderId aId, int aDataLength, const uint8_t* aData)
    : mId(aId)
    , mDataLength(aDataLength)
    , mData(nullptr)
  {
    mData.reset(new uint8_t[mDataLength]);
    memcpy(mData.get(), aData, aDataLength);
  }

  ~ObexHeader()
  {
  }

  ObexHeaderId mId;
  int mDataLength;
  UniquePtr<uint8_t[]> mData;
};

class ObexHeaderSet
{
public:
  ObexHeaderSet()
  {
  }

  ~ObexHeaderSet()
  {
  }

  void AddHeader(ObexHeader* aHeader)
  {
    mHeaders.AppendElement(aHeader);
  }

  void GetName(nsString& aRetName) const
  {
    aRetName.Truncate();

    int length = mHeaders.Length();

    for (int i = 0; i < length; ++i) {
      if (mHeaders[i]->mId == ObexHeaderId::Name) {
        /*
         * According to section 2.2.2 [Name] of IrOBEX spec, we know that
         * the Name header is "a null terminated Unicode text string describing
         * the name of the object.", and that's the reason why we need to minus
         * 1 to get the real length of the file name.
         */
        int nameLength = mHeaders[i]->mDataLength / 2 - 1;
        uint8_t* ptr = mHeaders[i]->mData.get();

        for (int j = 0; j < nameLength; ++j) {
          char16_t c = BigEndian::readUint16(&ptr[j * 2]);
          aRetName += c;
        }

        break;
      }
    }
  }

  void GetContentType(nsString& aRetContentType) const
  {
    aRetContentType.Truncate();

    int length = mHeaders.Length();

    for (int i = 0; i < length; ++i) {
      if (mHeaders[i]->mId == ObexHeaderId::Type) {
        uint8_t* ptr = mHeaders[i]->mData.get();
        aRetContentType.AssignASCII((const char*)ptr);
        break;
      }
    }
  }

  // @return file length, 0 means file length is unknown.
  void GetLength(uint32_t* aRetLength) const
  {
    int length = mHeaders.Length();
    *aRetLength = 0;

    for (int i = 0; i < length; ++i) {
      if (mHeaders[i]->mId == ObexHeaderId::Length) {
        uint8_t* ptr = mHeaders[i]->mData.get();
        *aRetLength = BigEndian::readUint32(&ptr[0]);
        return;
      }
    }
  }

  void GetBody(uint8_t** aRetBody, int* aRetBodyLength) const
  {
    int length = mHeaders.Length();
    *aRetBody = nullptr;
    *aRetBodyLength = 0;

    for (int i = 0; i < length; ++i) {
      if (mHeaders[i]->mId == ObexHeaderId::Body ||
          mHeaders[i]->mId == ObexHeaderId::EndOfBody) {
        uint8_t* ptr = mHeaders[i]->mData.get();
        *aRetBody = new uint8_t[mHeaders[i]->mDataLength];
        memcpy(*aRetBody, ptr, mHeaders[i]->mDataLength);
        *aRetBodyLength = mHeaders[i]->mDataLength;
        return;
      }
    }
  }

  uint32_t GetConnectionId() const
  {
    int length = mHeaders.Length();

    for (int i = 0; i < length; ++i) {
      if (mHeaders[i]->mId == ObexHeaderId::ConnectionId) {
        uint32_t* id = (uint32_t *) mHeaders[i]->mData.get();
        return *id;
      }
    }

    // According to OBEX spec., the value 0xFFFFFFFF is reserved and it's
    // considered invalid for Connection ID.
    return 0xFFFFFFFF;
  }

  /**
   * Get a specified parameter from the 'Application Parameters' header with
   * big-endian byte ordering.
   *
   * @param aTagId      [in]  The tag ID of parameter which is defined by
   *                          applications or upper protocol layer.
   * @param aRetBuf     [out] The buffer which is used to return the parameter.
   * @param aBufferSize [in]  The size of the given buffer.
   *
   * @return a boolean value to indicate whether the given paramter exists.
   */
  bool GetAppParameter(uint8_t aTagId, uint8_t* aRetBuf, int aBufferSize) const
  {
    int length = mHeaders.Length();

    for (int i = 0; i < length; ++i) {
      // Parse the 'Application Parameters' header.
      if (mHeaders[i]->mId == ObexHeaderId::AppParameters) {
        uint8_t* ptr = mHeaders[i]->mData.get();
        int dataLen = mHeaders[i]->mDataLength;

        // An application parameters header may contain more than one
        // [tag]-[length]-[value] triplet. The [tag] and [length] fields are
        // each one byte in length.
        uint8_t tagId;
        uint8_t offset = 0;
        do {
          tagId = *(ptr + offset++);

          // The length of value field, it should not exceed 255.
          uint8_t paramLen = *(ptr + offset++);

          if (tagId == aTagId) {
            memcpy(aRetBuf, ptr + offset, paramLen < aBufferSize ? paramLen
                                                                 : aBufferSize);
            return true;
          }

          offset += paramLen;
        } while (offset < dataLen);
      }
    }

    // The specified parameter don't exist in 'Application Parameters' header.
    return false;
  }

  const ObexHeader* GetHeader(ObexHeaderId aId) const
  {
    for (int i = 0, length = mHeaders.Length(); i < length; ++i) {
      if (mHeaders[i]->mId == aId) {
        return mHeaders[i].get();
      }
    }

    return nullptr;
  }

  bool Has(ObexHeaderId aId) const
  {
    return !!GetHeader(aId);
  }

  void ClearHeaders()
  {
    mHeaders.Clear();
  }

private:
  nsTArray<UniquePtr<ObexHeader> > mHeaders;
};

int AppendHeaderName(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aName,
                     int aLength);
int AppendHeaderBody(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aBody,
                     int aLength);
int AppendHeaderTarget(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aTarget,
                       int aLength);
int AppendHeaderWho(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aWho,
                    int aLength);
int AppendAuthResponse(uint8_t* aRetBuf, int aBufferSize,
                       const uint8_t* aDigest, int aLength);
int AppendHeaderAppParameters(uint8_t* aRetBuf, int aBufferSize,
                              const uint8_t* aAppParameters, int aLength);
int AppendAppParameter(uint8_t* aRetBuf, int aBufferSize, const uint8_t aTagId,
                       const uint8_t* aValue, int aLength);
int AppendHeaderLength(uint8_t* aRetBuf, int aObjectLength);
int AppendHeaderConnectionId(uint8_t* aRetBuf, int aConnectionId);
int AppendHeaderEndOfBody(uint8_t* aRetBuf);
void SetObexPacketInfo(uint8_t* aRetBuf, uint8_t aOpcode, int aPacketLength);

/**
 * @return true when the message was parsed without any error, false otherwise.
 */
bool ParseHeaders(const uint8_t* aHeaderStart,
                  int aTotalLength,
                  ObexHeaderSet* aRetHanderSet);

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_ObexBase_h
