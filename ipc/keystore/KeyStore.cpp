/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <sys/stat.h>

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

#include "KeyStore.h"
#include "jsfriendapi.h"
#include "MainThreadUtils.h" // For NS_IsMainThread.

#include "plbase64.h"
#include "certdb.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace ipc {

static const char* KEYSTORE_SOCKET_NAME = "keystore";
static const char* KEYSTORE_SOCKET_PATH = "/dev/socket/keystore";

int
KeyStoreConnector::Create()
{
  MOZ_ASSERT(!NS_IsMainThread());

  int fd;

  unlink(KEYSTORE_SOCKET_PATH);

  fd = socket(AF_LOCAL, SOCK_STREAM, 0);

  if (fd < 0) {
    NS_WARNING("Could not open keystore socket!");
    return -1;
  }

  return fd;
}

bool
KeyStoreConnector::CreateAddr(bool aIsServer,
                              socklen_t& aAddrSize,
                              sockaddr_any& aAddr,
                              const char* aAddress)
{
  // Keystore socket must be server
  MOZ_ASSERT(aIsServer);

  aAddr.un.sun_family = AF_LOCAL;
  if(strlen(KEYSTORE_SOCKET_PATH) > sizeof(aAddr.un.sun_path)) {
      NS_WARNING("Address too long for socket struct!");
      return false;
  }
  strcpy((char*)&aAddr.un.sun_path, KEYSTORE_SOCKET_PATH);
  aAddrSize = strlen(KEYSTORE_SOCKET_PATH) + offsetof(struct sockaddr_un, sun_path) + 1;

  return true;
}

bool
KeyStoreConnector::SetUp(int aFd)
{
  return true;
}

bool
KeyStoreConnector::SetUpListenSocket(int aFd)
{
  // Allow access of wpa_supplicant(different user, differnt group)
  chmod(KEYSTORE_SOCKET_PATH, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

  return true;
}

void
KeyStoreConnector::GetSocketAddr(const sockaddr_any& aAddr,
                                 nsAString& aAddrStr)
{
  // Unused.
  MOZ_CRASH("This should never be called!");
}

static char *
get_cert_db_filename(void *arg, int vers)
{
  static char keystoreDbPath[] = "/data/misc/wifi/keystore";
  return keystoreDbPath;
}

KeyStore::KeyStore()
{
  // Initial NSS
  certdb = CERT_GetDefaultCertDB();

  Listen();
}

void
KeyStore::Shutdown()
{
  mShutdown = true;
  CloseSocket();
}

void
KeyStore::Listen()
{
  ListenSocket(new KeyStoreConnector());

  ResetHandlerInfo();
}

void
KeyStore::ResetHandlerInfo()
{
  mHandlerInfo.state = STATE_IDLE;
  mHandlerInfo.command = 0;
  mHandlerInfo.paramCount = 0;
  mHandlerInfo.commandPattern = nullptr;
  for (int i = 0; i < MAX_PARAM; i++) {
    mHandlerInfo.param[i].length = 0;
    memset(mHandlerInfo.param[i].data, 0, VALUE_SIZE);
  }
}

bool
KeyStore::CheckSize(UnixSocketRawData *aMessage, size_t aExpectSize)
{
  return (aMessage->mSize - aMessage->mCurrentWriteOffset >= aExpectSize) ?
         true : false;
}

bool
KeyStore::ReadCommand(UnixSocketRawData *aMessage)
{
  if (mHandlerInfo.state != STATE_IDLE) {
    NS_WARNING("Wrong state in ReadCommand()!");
    return false;
  }

  if (!CheckSize(aMessage, 1)) {
    NS_WARNING("Data size error in ReadCommand()!");
    return false;
  }

  mHandlerInfo.command = aMessage->mData[aMessage->mCurrentWriteOffset];
  aMessage->mCurrentWriteOffset++;

  // Find corrsponding command pattern
  const struct ProtocolCommand *command = commands;
  while (command->command && command->command != mHandlerInfo.command) {
    command++;
  }

  if (!command->command) {
    NS_WARNING("Unsupported command!");
    return false;
  }

  // Get command pattern.
  mHandlerInfo.commandPattern = command;
  if (command->paramNum) {
    // Read command parameter if needed.
    mHandlerInfo.state = STATE_READ_PARAM_LEN;
  } else {
    mHandlerInfo.state = STATE_PROCESSING;
  }

  return true;
}

bool
KeyStore::ReadLength(UnixSocketRawData *aMessage)
{
  if (mHandlerInfo.state != STATE_READ_PARAM_LEN) {
    NS_WARNING("Wrong state in ReadLength()!");
    return false;
  }

  if (!CheckSize(aMessage, 2)) {
    NS_WARNING("Data size error in ReadLength()!");
    return false;
  }

  // Read length of command parameter.
  unsigned short dataLength;
  memcpy(&dataLength, &aMessage->mData[aMessage->mCurrentWriteOffset], 2);
  aMessage->mCurrentWriteOffset += 2;
  mHandlerInfo.param[mHandlerInfo.paramCount].length = ntohs(dataLength);

  mHandlerInfo.state = STATE_READ_PARAM_DATA;

  return true;
}

bool
KeyStore::ReadData(UnixSocketRawData *aMessage)
{
  if (mHandlerInfo.state != STATE_READ_PARAM_DATA) {
    NS_WARNING("Wrong state in ReadData()!");
    return false;
  }

  if (!CheckSize(aMessage, mHandlerInfo.param[mHandlerInfo.paramCount].length)) {
    NS_WARNING("Data size error in ReadData()!");
    return false;
  }

  // Read command parameter.
  memcpy(mHandlerInfo.param[mHandlerInfo.paramCount].data,
         &aMessage->mData[aMessage->mCurrentWriteOffset],
         mHandlerInfo.param[mHandlerInfo.paramCount].length);
  aMessage->mCurrentWriteOffset += mHandlerInfo.param[mHandlerInfo.paramCount].length;
  mHandlerInfo.paramCount++;

  if (mHandlerInfo.paramCount == mHandlerInfo.commandPattern->paramNum) {
    mHandlerInfo.state = STATE_PROCESSING;
  } else {
    mHandlerInfo.state = STATE_READ_PARAM_LEN;
  }

  return true;
}

// Transform base64 certification data into DER format
void
KeyStore::FormatCaData(const uint8_t *aCaData, int aCaDataLength,
                       const char *aName, const uint8_t **aFormatData,
                       int &aFormatDataLength)
{
  int bufSize = strlen(CA_BEGIN) + strlen(CA_END) + strlen(CA_TAILER) * 2 +
                strlen(aName) * 2 + aCaDataLength + aCaDataLength/CA_LINE_SIZE + 2;
  char *buf = (char *)malloc(bufSize);

  aFormatDataLength = bufSize;
  *aFormatData = (const uint8_t *)buf;

  char *ptr = buf;
  int len;

  // Create DER header.
  len = snprintf(ptr, bufSize, "%s%s%s", CA_BEGIN, aName, CA_TAILER);
  ptr += len;
  bufSize -= len;

  // Split base64 data in lines.
  int copySize;
  while (aCaDataLength > 0) {
    copySize = (aCaDataLength > CA_LINE_SIZE) ? CA_LINE_SIZE : aCaDataLength;

    memcpy(ptr, aCaData, copySize);
    ptr += copySize;
    aCaData += copySize;
    aCaDataLength -= copySize;
    bufSize -= copySize;

    *ptr = '\n';
    ptr++;
    bufSize--;
  }

  // Create DEA tailer.
  snprintf(ptr, bufSize, "%s%s%s", CA_END, aName, CA_TAILER);
}

// Status response
void
KeyStore::SendResponse(ResponseCode aResponse)
{
  if (aResponse == NO_RESPONSE)
    return;

  uint8_t response = (uint8_t)aResponse;
  UnixSocketRawData* data = new UnixSocketRawData((const void *)&response, 1);
  SendSocketData(data);
}

// Data response
void
KeyStore::SendData(const uint8_t *aData, int aLength)
{
  unsigned short dataLength = htons(aLength);

  UnixSocketRawData* length = new UnixSocketRawData((const void *)&dataLength, 2);
  SendSocketData(length);

  UnixSocketRawData* data = new UnixSocketRawData((const void *)aData, aLength);
  SendSocketData(data);
}

void
KeyStore::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  bool success = true;
  while (aMessage->mCurrentWriteOffset < aMessage->mSize ||
         mHandlerInfo.state == STATE_PROCESSING) {
    switch (mHandlerInfo.state) {
      case STATE_IDLE:
        success = ReadCommand(aMessage);
        break;
      case STATE_READ_PARAM_LEN:
        success = ReadLength(aMessage);
        break;
      case STATE_READ_PARAM_DATA:
        success = ReadData(aMessage);
        break;
      case STATE_PROCESSING:
        success = false;
        if (mHandlerInfo.command == 'g') {
          // Get CA
          const uint8_t *certData;
          int certDataLength;
          const char *certName = (const char *)mHandlerInfo.param[0].data;

          // Get cert from NSS by name
          CERTCertificate *cert = CERT_FindCertByNickname(certdb, certName);
          if (!cert) {
            break;
          }

          char *certDER = PL_Base64Encode((const char *)cert->derCert.data,
                                          cert->derCert.len, nullptr);
          if (!certDER) {
            break;
          }

          FormatCaData((const uint8_t *)certDER, strlen(certDER), "CERTIFICATE",
                       &certData, certDataLength);
          PL_strfree(certDER);

          SendResponse(SUCCESS);
          SendData(certData, certDataLength);
          success = true;

          free((void *)certData);
        }

        ResetHandlerInfo();
        break;
    }

    if (!success) {
      SendResponse(PROTOCOL_ERROR);
      ResetHandlerInfo();
      return;
    }
  }
}

void
KeyStore::OnConnectSuccess()
{
  mShutdown = false;
}

void
KeyStore::OnConnectError()
{
  if (!mShutdown) {
    Listen();
  }
}

void
KeyStore::OnDisconnect()
{
  if (!mShutdown) {
    Listen();
  }
}

} // namespace ipc
} // namespace mozilla
