/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ipcm_h__
#define ipcm_h__

#include "ipcMessage.h"
#include "ipcMessagePrimitives.h"

//-----------------------------------------------------------------------------

//
// IPCM (IPC Manager) protocol support
//

// The IPCM message target identifier:
extern const nsID IPCM_TARGET;

//
// Every IPCM message has the following structure:
//
//  +-----------------------------------------+
//  | (ipc message header)                    |
//  +-----------------------------------------+
//  | DWORD : type                            |
//  +-----------------------------------------+
//  | DWORD : requestIndex                    |
//  +-----------------------------------------+
//  .                                         .
//  . (payload)                               .
//  .                                         .
//  +-----------------------------------------+
//
// where |type| is an integer uniquely identifying the message.  the type is
// composed of a message class identifier and a message number.  there are 3
// message classes:
//
//   ACK - acknowledging a request
//   REQ - making a request
//   PSH - providing unrequested, "pushed" information
//
// The requestIndex field is initialized when a request is made.  An
// acknowledgement's requestIndex is equal to that of its corresponding
// request message.  This enables the requesting side of the message exchange
// to match acknowledgements to requests.  The requestIndex field is ignored
// for PSH messages.
//

// The IPCM message class is stored in the most significant byte.
#define IPCM_MSG_CLASS_REQ (1 << 24)
#define IPCM_MSG_CLASS_ACK (2 << 24)
#define IPCM_MSG_CLASS_PSH (4 << 24)

// Requests
#define IPCM_MSG_REQ_PING                   (IPCM_MSG_CLASS_REQ | 1)
#define IPCM_MSG_REQ_FORWARD                (IPCM_MSG_CLASS_REQ | 2)
#define IPCM_MSG_REQ_CLIENT_HELLO           (IPCM_MSG_CLASS_REQ | 3)
#define IPCM_MSG_REQ_CLIENT_ADD_NAME        (IPCM_MSG_CLASS_REQ | 4)
#define IPCM_MSG_REQ_CLIENT_DEL_NAME        (IPCM_MSG_CLASS_REQ | 5)
#define IPCM_MSG_REQ_CLIENT_ADD_TARGET      (IPCM_MSG_CLASS_REQ | 6)
#define IPCM_MSG_REQ_CLIENT_DEL_TARGET      (IPCM_MSG_CLASS_REQ | 7)
#define IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME   (IPCM_MSG_CLASS_REQ | 8)
#define IPCM_MSG_REQ_QUERY_CLIENT_NAMES     (IPCM_MSG_CLASS_REQ | 9)  // TODO
#define IPCM_MSG_REQ_QUERY_CLIENT_TARGETS   (IPCM_MSG_CLASS_REQ | 10) // TODO

// Acknowledgements
#define IPCM_MSG_ACK_RESULT                 (IPCM_MSG_CLASS_ACK | 1)
#define IPCM_MSG_ACK_CLIENT_ID              (IPCM_MSG_CLASS_ACK | 2)
#define IPCM_MSG_ACK_CLIENT_NAMES           (IPCM_MSG_CLASS_ACK | 3)  // TODO
#define IPCM_MSG_ACK_CLIENT_TARGETS         (IPCM_MSG_CLASS_ACK | 4)  // TODO

// Push messages
#define IPCM_MSG_PSH_CLIENT_STATE           (IPCM_MSG_CLASS_PSH | 1)
#define IPCM_MSG_PSH_FORWARD                (IPCM_MSG_CLASS_PSH | 2)

//-----------------------------------------------------------------------------

//
// IPCM header
//
struct ipcmMessageHeader
{
    PRUint32 mType;
    PRUint32 mRequestIndex;
};

//
// returns IPCM message type.
//
static inline int
IPCM_GetType(const ipcMessage *msg)
{
    return ((const ipcmMessageHeader *) msg->Data())->mType;
}

//
// return IPCM message request index.  
//
static inline PRUint32
IPCM_GetRequestIndex(const ipcMessage *msg)
{
    return ((const ipcmMessageHeader *) msg->Data())->mRequestIndex;
}

//
// return a request index that is unique to this process.
//
NS_HIDDEN_(PRUint32)
IPCM_NewRequestIndex();

//-----------------------------------------------------------------------------

//
// The IPCM protocol is detailed below:
//

// REQUESTS

//
// req: IPCM_MSG_REQ_PING
// ack: IPCM_MSG_ACK_RESULT
//
// A PING can be sent from either a client to the daemon, or from the daemon
// to a client.  The expected acknowledgement is a RESULT message with a status
// code of 0.
//
// This request message has no payload.
//

//
// req: IPCM_MSG_REQ_FORWARD
// ack: IPCM_MSG_ACK_RESULT
//
// A FORWARD is sent when a client wishes to send a message to another client.
// The payload of this message is another message that should be forwarded by
// the daemon's IPCM to the specified client.  The expected acknowledgment is
// a RESULT message with a status code indicating success or failure.
//
// When the daemon receives a FORWARD message, it creates a PSH_FORWARD message
// and sends that on to the destination client.
//
// This request message has as its payload:
//
//  +-----------------------------------------+
//  | DWORD : clientID                        |
//  +-----------------------------------------+
//  | (innerMsgHeader)                        |
//  +-----------------------------------------+
//  | (innerMsgData)                          |
//  +-----------------------------------------+
//

//
// req: IPCM_MSG_REQ_CLIENT_HELLO
// ack: IPCM_MSG_REQ_CLIENT_ID <or> IPCM_MSG_REQ_RESULT
//
// A CLIENT_HELLO is sent when a client connects to the IPC daemon.  The
// expected acknowledgement is a CLIENT_ID message informing the new client of
// its ClientID.  If for some reason the IPC daemon cannot accept the new
// client, it returns a RESULT message with a failure status code.
//
// This request message has no payload.
//

//
// req: IPCM_MSG_REQ_CLIENT_ADD_NAME
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_ADD_NAME is sent when a client wishes to register an additional
// name for itself.  The expected acknowledgement is a RESULT message with a
// status code indicating success or failure.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the name of the client.
//

//
// req: IPCM_MSG_REQ_CLIENT_DEL_NAME
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_DEL_NAME is sent when a client wishes to unregister a name that it
// has registered.  The expected acknowledgement is a RESULT message with a
// status code indicating success or failure.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the name of the client.
//

//
// req: IPCM_MSG_REQ_CLIENT_ADD_TARGET
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_ADD_TARGET is sent when a client wishes to register an additional
// target that it supports.  The expected acknowledgement is a RESULT message
// with a status code indicating success or failure.
//
// This request message has as its payload a 128-bit UUID indicating the
// target to add.
//

//
// req: IPCM_MSG_REQ_CLIENT_DEL_TARGET
// ack: IPCM_MSG_ACK_RESULT
//
// A CLIENT_DEL_TARGET is sent when a client wishes to unregister a target
// that it has registered.  The expected acknowledgement is a RESULT message
// with a status code indicating success or failure.
//
// This request message has as its payload a 128-bit UUID indicating the
// target to remove.
//

//
// req: IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME
// ack: IPCM_MSG_ACK_CLIENT_ID <or> IPCM_MSG_ACK_RESULT
//
// A QUERY_CLIENT_BY_NAME may be sent by a client to discover the client that
// is known by a common name.  If more than one client matches the name, then
// only the ID of the more recently registered client is returned.  The
// expected acknowledgement is a CLIENT_ID message carrying the ID of the
// corresponding client.  If no client matches the given name or if some error
// occurs, then a RESULT message with a failure status code is returned.
//
// This request message has as its payload a null-terminated ASCII character
// string indicating the client name to query.
// 

// ACKNOWLEDGEMENTS

//
// ack: IPCM_MSG_ACK_RESULT
//
// This acknowledgement is returned to indicate a success or failure status.
//
// The payload consists of a single DWORD value.
//
// Possible status codes are listed below (negative values indicate failure
// codes):
//
#define IPCM_OK               0  // success: generic
#define IPCM_ERROR_GENERIC   -1  // failure: generic
#define IPCM_ERROR_NO_CLIENT -2  // failure: client does not exist

//
// ack: IPCM_MSG_ACK_CLIENT_ID
//
// This acknowledgement is returned to specify a client ID.
//
// The payload consists of a single DWORD value.
//

// PUSH MESSAGES

//
// psh: ICPM_MSG_PSH_CLIENT_STATE
//
// This message is sent to clients to indicate the status of other clients.
//
// The payload consists of:
//
//  +-----------------------------------------+
//  | DWORD : clientID                        |
//  +-----------------------------------------+
//  | DWORD : clientState                     |
//  +-----------------------------------------+
//
// where, clientState is one of the following values indicating whether the
// client has recently connected (up) or disconnected (down):
//
#define IPCM_CLIENT_STATE_UP     1
#define IPCM_CLIENT_STATE_DOWN   2

//
// psh: IPCM_MSG_PSH_FORWARD
//
// This message is sent by the daemon to a client on behalf of another client.
// The recipient is expected to unpack the contained message and process it.
// 
// The payload of this message matches the payload of IPCM_MSG_REQ_FORWARD,
// with the exception that the clientID field is set to the clientID of the
// sender of the IPCM_MSG_REQ_FORWARD message.
//

//-----------------------------------------------------------------------------

//
// NOTE: This file declares some helper classes that simplify constructing
//       and parsing IPCM messages.  Each class subclasses ipcMessage, but
//       adds no additional member variables.  |operator new| should be used
//       to allocate one of the IPCM helper classes, e.g.:
//         
//          ipcMessage *msg = new ipcmMessageClientHello("foo");
//
//       Given an arbitrary ipcMessage, it can be parsed using logic similar
//       to the following:
//
//          void func(const ipcMessage *unknown)
//          {
//            if (unknown->Topic().Equals(IPCM_TARGET)) {
//              if (IPCM_GetMsgType(unknown) == IPCM_MSG_TYPE_CLIENT_ID) {
//                ipcMessageCast<ipcmMessageClientID> msg(unknown);
//                printf("Client ID: %u\n", msg->ClientID());
//              }
//            }
//          }
//

// REQUESTS

class ipcmMessagePing : public ipcMessage_DWORD_DWORD
{
public:
    ipcmMessagePing()
        : ipcMessage_DWORD_DWORD(
            IPCM_TARGET,
            IPCM_MSG_REQ_PING,
            IPCM_NewRequestIndex()) {}
};

class ipcmMessageForward : public ipcMessage
{
public:
    // @param type        the type of this message: IPCM_MSG_{REQ,PSH}_FORWARD
    // @param clientID    the client id of the sender or receiver
    // @param target      the message target
    // @param data        the message data
    // @param dataLen     the message data length
    ipcmMessageForward(PRUint32 type,
                       PRUint32 clientID,
                       const nsID &target,
                       const char *data,
                       PRUint32 dataLen) NS_HIDDEN;

    // set inner message data, constrained to the data length passed
    // to this class's constructor.
    NS_HIDDEN_(void) SetInnerData(PRUint32 offset, const char *data, PRUint32 dataLen);

    NS_HIDDEN_(PRUint32)     ClientID() const;
    NS_HIDDEN_(const nsID &) InnerTarget() const;
    NS_HIDDEN_(const char *) InnerData() const;
    NS_HIDDEN_(PRUint32)     InnerDataLen() const;
};

class ipcmMessageClientHello : public ipcMessage_DWORD_DWORD
{
public:
    ipcmMessageClientHello()
        : ipcMessage_DWORD_DWORD(
            IPCM_TARGET,
            IPCM_MSG_REQ_CLIENT_HELLO,
            IPCM_NewRequestIndex()) {}
};

class ipcmMessageClientAddName : public ipcMessage_DWORD_DWORD_STR
{
public:
    ipcmMessageClientAddName(const char *name)
        : ipcMessage_DWORD_DWORD_STR(
            IPCM_TARGET,
            IPCM_MSG_REQ_CLIENT_ADD_NAME,
            IPCM_NewRequestIndex(),
            name) {}

    const char *Name() const { return Third(); }
};

class ipcmMessageClientDelName : public ipcMessage_DWORD_DWORD_STR
{
public:
    ipcmMessageClientDelName(const char *name)
        : ipcMessage_DWORD_DWORD_STR(
            IPCM_TARGET,
            IPCM_MSG_REQ_CLIENT_DEL_NAME,
            IPCM_NewRequestIndex(),
            name) {}

    const char *Name() const { return Third(); }
};

class ipcmMessageClientAddTarget : public ipcMessage_DWORD_DWORD_ID
{
public:
    ipcmMessageClientAddTarget(const nsID &target)
        : ipcMessage_DWORD_DWORD_ID(
            IPCM_TARGET,
            IPCM_MSG_REQ_CLIENT_ADD_TARGET,
            IPCM_NewRequestIndex(),
            target) {}

    const nsID &Target() const { return Third(); }
};

class ipcmMessageClientDelTarget : public ipcMessage_DWORD_DWORD_ID
{
public:
    ipcmMessageClientDelTarget(const nsID &target)
        : ipcMessage_DWORD_DWORD_ID(
            IPCM_TARGET,
            IPCM_MSG_REQ_CLIENT_ADD_TARGET,
            IPCM_NewRequestIndex(),
            target) {}

    const nsID &Target() const { return Third(); }
};

class ipcmMessageQueryClientByName : public ipcMessage_DWORD_DWORD_STR
{
public:
    ipcmMessageQueryClientByName(const char *name)
        : ipcMessage_DWORD_DWORD_STR(
            IPCM_TARGET,
            IPCM_MSG_REQ_QUERY_CLIENT_BY_NAME,
            IPCM_NewRequestIndex(),
            name) {}

    const char *Name() const { return Third(); }
    PRUint32 RequestIndex() const { return Second(); }
};

// ACKNOWLEDGEMENTS

class ipcmMessageResult : public ipcMessage_DWORD_DWORD_DWORD
{
public:
    ipcmMessageResult(PRUint32 requestIndex, PRInt32 status)
        : ipcMessage_DWORD_DWORD_DWORD(
            IPCM_TARGET,
            IPCM_MSG_ACK_RESULT,
            requestIndex,
            (PRUint32) status) {}

    PRInt32 Status() const { return (PRInt32) Third(); }
};

class ipcmMessageClientID : public ipcMessage_DWORD_DWORD_DWORD
{
public:
    ipcmMessageClientID(PRUint32 requestIndex, PRUint32 clientID)
        : ipcMessage_DWORD_DWORD_DWORD(
            IPCM_TARGET,
            IPCM_MSG_ACK_CLIENT_ID,
            requestIndex,
            clientID) {}

    PRUint32 ClientID() const { return Third(); }
};

// PUSH MESSAGES

class ipcmMessageClientState : public ipcMessage_DWORD_DWORD_DWORD_DWORD
{
public:
    ipcmMessageClientState(PRUint32 clientID, PRUint32 clientStatus)
        : ipcMessage_DWORD_DWORD_DWORD_DWORD(
            IPCM_TARGET,
            IPCM_MSG_PSH_CLIENT_STATE,
            0,
            clientID,
            clientStatus) {}

    PRUint32 ClientID() const { return Third(); }
    PRUint32 ClientState() const { return Fourth(); }
};

#endif // !ipcm_h__
