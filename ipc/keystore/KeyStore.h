/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_KeyStore_h
#define mozilla_ipc_KeyStore_h 1

#include "mozilla/ipc/UnixSocket.h"
#include <sys/socket.h>
#include <sys/un.h>

#include "cert.h"

namespace mozilla {
namespace ipc {

enum ResponseCode {
  SUCCESS           =  1,
  LOCKED            =  2,
  UNINITIALIZED     =  3,
  SYSTEM_ERROR      =  4,
  PROTOCOL_ERROR    =  5,
  PERMISSION_DENIED =  6,
  KEY_NOT_FOUND     =  7,
  VALUE_CORRUPTED   =  8,
  UNDEFINED_ACTION  =  9,
  WRONG_PASSWORD_0  = 10,
  WRONG_PASSWORD_1  = 11,
  WRONG_PASSWORD_2  = 12,
  WRONG_PASSWORD_3  = 13, // MAX_RETRY = 4
  NO_RESPONSE
};

void FormatCaData(const uint8_t *aCaData, int aCaDataLength,
                  const char *aName, const uint8_t **aFormatData,
                  int *aFormatDataLength);

ResponseCode getCertificate(const char *aCertName, const uint8_t **aCertData,
                            int *aCertDataLength);

bool checkPermission(uid_t uid);

static const int MAX_PARAM = 2;
static const int KEY_SIZE = ((NAME_MAX - 15) / 2);
static const int VALUE_SIZE = 32768;
static const int PASSWORD_SIZE = VALUE_SIZE;

static const char *CA_BEGIN = "-----BEGIN ",
                  *CA_END   = "-----END ",
                  *CA_TAILER = "-----\n";
static const int CA_LINE_SIZE = 64;

struct ProtocolCommand {
  int8_t  command;
  int     paramNum;
};

static const struct ProtocolCommand commands[] = {
  {'g', 1}, // Get CA, command "g CERT_NAME"
  { 0,  0}
};

struct ProtocolParam{
  uint    length;
  int8_t  data[VALUE_SIZE];
};

typedef enum {
  STATE_IDLE,
  STATE_READ_PARAM_LEN,
  STATE_READ_PARAM_DATA,
  STATE_PROCESSING
} ProtocolHandlerState;

class KeyStoreConnector : public mozilla::ipc::UnixSocketConnector
{
public:
  KeyStoreConnector()
  {}

  virtual ~KeyStoreConnector()
  {}

  virtual int Create();
  virtual bool CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          sockaddr_any& aAddr,
                          const char* aAddress);
  virtual bool SetUp(int aFd);
  virtual bool SetUpListenSocket(int aFd);
  virtual void GetSocketAddr(const sockaddr_any& aAddr,
                             nsAString& aAddrStr);
};

class KeyStore : public mozilla::ipc::UnixSocketConsumer
{
public:
  KeyStore();

  void Shutdown();

private:
  virtual ~KeyStore();

  virtual void ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage);

  virtual void OnConnectSuccess();
  virtual void OnConnectError();
  virtual void OnDisconnect();

  struct {
    ProtocolHandlerState          state;
    uint8_t                       command;
    struct ProtocolParam          param[MAX_PARAM];
    int                           paramCount;
    const struct ProtocolCommand  *commandPattern;
  } mHandlerInfo;
  void ResetHandlerInfo();
  void Listen();

  bool CheckSize(UnixSocketRawData *aMessage, size_t aExpectSize);
  ResponseCode ReadCommand(UnixSocketRawData *aMessage);
  ResponseCode ReadLength(UnixSocketRawData *aMessage);
  ResponseCode ReadData(UnixSocketRawData *aMessage);
  void SendResponse(ResponseCode response);
  void SendData(const uint8_t *data, int length);

  bool mShutdown;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_KeyStore_h
