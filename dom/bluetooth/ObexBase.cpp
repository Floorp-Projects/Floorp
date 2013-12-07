/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObexBase.h"

BEGIN_BLUETOOTH_NAMESPACE

int
AppendHeaderName(uint8_t* aRetBuf, const char* aName, int aLength)
{
  int headerLength = aLength + 3;

  aRetBuf[0] = ObexHeaderId::Name;
  aRetBuf[1] = (headerLength & 0xFF00) >> 8;
  aRetBuf[2] = headerLength & 0x00FF;

  memcpy(&aRetBuf[3], aName, aLength);

  return headerLength;
}

int
AppendHeaderBody(uint8_t* aRetBuf, uint8_t* aData, int aLength)
{
  int headerLength = aLength + 3;

  aRetBuf[0] = ObexHeaderId::Body;
  aRetBuf[1] = (headerLength & 0xFF00) >> 8;
  aRetBuf[2] = headerLength & 0x00FF;

  memcpy(&aRetBuf[3], aData, aLength);

  return headerLength;
}

int
AppendHeaderEndOfBody(uint8_t* aRetBuf)
{
  aRetBuf[0] = ObexHeaderId::EndOfBody;
  aRetBuf[1] = 0x00;
  aRetBuf[2] = 0x03;

  return 3;
}

int
AppendHeaderLength(uint8_t* aRetBuf, int aObjectLength)
{
  aRetBuf[0] = ObexHeaderId::Length;
  aRetBuf[1] = (aObjectLength & 0xFF000000) >> 24;
  aRetBuf[2] = (aObjectLength & 0x00FF0000) >> 16;
  aRetBuf[3] = (aObjectLength & 0x0000FF00) >> 8;
  aRetBuf[4] = aObjectLength & 0x000000FF;

  return 5;
}

int
AppendHeaderConnectionId(uint8_t* aRetBuf, int aConnectionId)
{
  aRetBuf[0] = ObexHeaderId::ConnectionId;
  aRetBuf[1] = (aConnectionId & 0xFF000000) >> 24;
  aRetBuf[2] = (aConnectionId & 0x00FF0000) >> 16;
  aRetBuf[3] = (aConnectionId & 0x0000FF00) >> 8;
  aRetBuf[4] = aConnectionId & 0x000000FF;

  return 5;
}

void
SetObexPacketInfo(uint8_t* aRetBuf, uint8_t aOpcode, int aPacketLength)
{
  aRetBuf[0] = aOpcode;
  aRetBuf[1] = (aPacketLength & 0xFF00) >> 8;
  aRetBuf[2] = aPacketLength & 0x00FF;
}

bool
ParseHeaders(const uint8_t* aHeaderStart,
             int aTotalLength,
             ObexHeaderSet* aRetHandlerSet)
{
  const uint8_t* ptr = aHeaderStart;

  while (ptr - aHeaderStart < aTotalLength) {
    ObexHeaderId headerId = (ObexHeaderId)*ptr++;

    uint16_t contentLength = 0;
    uint8_t highByte, lowByte;

    // Defined in 2.1 OBEX Headers, IrOBEX 1.2
    switch (headerId >> 6)
    {
      case 0x00:
        // Null-terminated Unicode text, length prefixed with 2-byte
        // unsigned integer.
      case 0x01:
        // byte sequence, length prefixed with 2 byte unsigned integer.
        highByte = *ptr++;
        lowByte = *ptr++;
        contentLength = (((uint16_t)highByte << 8) | lowByte) - 3;
        break;

      case 0x02:
        // 1 byte quantity
        contentLength = 1;
        break;

      case 0x03:
        // 4 byte quantity
        contentLength = 4;
        break;
    }

    // Length check to prevent from memory pollusion.
    if (ptr + contentLength > aHeaderStart + aTotalLength) {
      // Severe error occurred. We can't even believe the received data, so
      // clear all headers.
      MOZ_ASSERT(false);
      aRetHandlerSet->ClearHeaders();
      return false;
    }

    aRetHandlerSet->AddHeader(new ObexHeader(headerId, contentLength, ptr));
    ptr += contentLength;
  }

  return true;
}

END_BLUETOOTH_NAMESPACE
