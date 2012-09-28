/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_obexbase_h__
#define mozilla_dom_bluetooth_obexbase_h__

#include "BluetoothCommon.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

BEGIN_BLUETOOTH_NAMESPACE

const char FINAL_BIT = 0x80;

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
  AuthChallenge =0x4D,
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

class ObexHeader {
public:
  ObexHeader(ObexHeaderId aId, int aDataLength, uint8_t* aData)
    : mId(aId)
    , mDataLength(aDataLength)
    , mData(nullptr)
  {
    mData = new uint8_t[mDataLength];
    memcpy(mData, aData, aDataLength);
  }

  ~ObexHeader()
  {
  }

  ObexHeaderId mId;
  int mDataLength;
  nsAutoPtr<uint8_t> mData;
};

class ObexHeaderSet {
public:
  uint8_t mOpcode;
  nsTArray<nsAutoPtr<ObexHeader> > mHeaders;

  ObexHeaderSet(uint8_t aOpcode) : mOpcode(aOpcode)
  {
  }

  ~ObexHeaderSet()
  {
  }

  void AddHeader(ObexHeader* aHeader)
  {
    mHeaders.AppendElement(aHeader);
  }
};

int AppendHeaderName(uint8_t* retBuf, const char* name, int length);
int AppendHeaderBody(uint8_t* retBuf, uint8_t* data, int length);
int AppendHeaderLength(uint8_t* retBuf, int objectLength);
int AppendHeaderConnectionId(uint8_t* retBuf, int connectionId);
void SetObexPacketInfo(uint8_t* retBuf, uint8_t opcode, int packetLength);
void ParseHeaders(uint8_t* buf, int totalLength, ObexHeaderSet* retHanderSet);

END_BLUETOOTH_NAMESPACE

#endif
