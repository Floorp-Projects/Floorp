/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_KeyStore_h
#define mozilla_ipc_KeyStore_h 1

#include <sys/socket.h>
#include <sys/un.h>
#include "cert.h"
#include "mozilla/ipc/ListenSocket.h"
#include "mozilla/ipc/StreamSocket.h"
#include "nsNSSShutDown.h"

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
                  size_t *aFormatDataLength);

ResponseCode getCertificate(const char *aCertName, const uint8_t **aCertData,
                            size_t *aCertDataLength);
ResponseCode getPrivateKey(const char *aKeyName, const uint8_t **aKeyData,
                           size_t *aKeyDataLength);
ResponseCode getPublicKey(const char *aKeyName, const uint8_t **aKeyData,
                          size_t *aKeyDataLength);
ResponseCode signData(const char *aKeyName, const uint8_t *data, size_t length,
                      uint8_t **out, size_t *outLength);

bool checkPermission(uid_t uid);

static const int MAX_PARAM = 2;
static const int KEY_SIZE = ((NAME_MAX - 15) / 2);
static const int VALUE_SIZE = 32768;
static const int PASSWORD_SIZE = VALUE_SIZE;

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

class KeyStore final : public nsNSSShutDownObject
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KeyStore)

  KeyStore();

  void Shutdown();

protected:
  virtual void virtualDestroyNSSReference() {}

private:
  enum SocketType {
    LISTEN_SOCKET,
    STREAM_SOCKET
  };

  class ListenSocket final : public mozilla::ipc::ListenSocket
  {
  public:
    ListenSocket(KeyStore* aKeyStore);
    ListenSocket();

    // SocketBase
    //

    void OnConnectSuccess() override;
    void OnConnectError() override;
    void OnDisconnect() override;

  private:
    KeyStore* mKeyStore;
  };

  class StreamSocket final : public mozilla::ipc::StreamSocket
  {
  public:
    StreamSocket(KeyStore* aKeyStore);
    ~StreamSocket();

    // SocketConsumerBase
    //

    void OnConnectSuccess() override;
    void OnConnectError() override;
    void OnDisconnect() override;

    void ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer) override;

    // ConnectionOrientedSocket
    //

    ConnectionOrientedSocketIO* GetIO() override;

  private:
    KeyStore* mKeyStore;
  };

  ~KeyStore();

  void ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aMessage);

  void OnConnectSuccess(enum SocketType aSocketType);
  void OnConnectError(enum SocketType aSocketType);
  void OnDisconnect(enum SocketType aSocketType);

  struct {
    ProtocolHandlerState          state;
    uint8_t                       command;
    struct ProtocolParam          param[MAX_PARAM];
    int                           paramCount;
    const struct ProtocolCommand  *commandPattern;
  } mHandlerInfo;
  void ResetHandlerInfo();
  void Listen();

  bool CheckSize(UnixSocketBuffer *aMessage, size_t aExpectSize);
  ResponseCode ReadCommand(UnixSocketBuffer *aMessage);
  ResponseCode ReadLength(UnixSocketBuffer *aMessage);
  ResponseCode ReadData(UnixSocketBuffer *aMessage);
  void SendResponse(ResponseCode response);
  void SendData(const uint8_t *data, int length);

  bool mShutdown;

  nsRefPtr<ListenSocket> mListenSocket;
  nsRefPtr<StreamSocket> mStreamSocket;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_KeyStore_h
