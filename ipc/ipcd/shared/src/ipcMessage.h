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

#ifndef ipcMessage_h__
#define ipcMessage_h__

#include "nsID.h"

//
// ipc message format:
//
//   +------------------------------------+
//   | DWORD : length                     |
//   +------------------+-----------------+
//   | WORD  : version  | WORD : flags    |
//   +------------------+-----------------+
//   | nsID  : target                     |
//   +------------------------------------+
//   | data                               |
//   +------------------------------------+
//
// header is 24 bytes.  flags are defined below.  default value of flags is
// zero.  protocol implementations should ignore unrecognized flags.  target
// is a 16 byte UUID indicating the intended receiver of this message.
//

struct ipcMessageHeader
{
    PRUint32 mLen;
    PRUint16 mVersion;
    PRUint16 mFlags;
    nsID     mTarget;
};

#define IPC_MSG_VERSION       (0x1)
#define IPC_MSG_HEADER_SIZE   (sizeof(ipcMessageHeader))
#define IPC_MSG_GUESSED_SIZE  (IPC_MSG_HEADER_SIZE + 64)

//
// the IPC message protocol supports synchronous messages.  these messages can
// only be sent from a client to the daemon.  a daemon module cannot send a
// synchronous message.  the client sets the SYNC_QUERY flag to indicate that
// it is expecting a response with the SYNC_REPLY flag set.
//
#define IPC_MSG_FLAG_SYNC_QUERY (0x1)
#define IPC_MSG_FLAG_SYNC_REPLY (0x2)

//-----------------------------------------------------------------------------
// ipcMessage
//-----------------------------------------------------------------------------

class ipcMessage
{
public:
    ipcMessage()
        : mNext(NULL)
        , mMetaData(0)
        , mMsgHdr(NULL)
        , mMsgOffset(0)
        , mMsgComplete(PR_FALSE)
        { }
    ipcMessage(const nsID &target, const char *data, PRUint32 dataLen)
        : mNext(NULL)
        , mMetaData(0)
        , mMsgHdr(NULL)
        , mMsgOffset(0)
        { Init(target, data, dataLen); }
   ~ipcMessage() NS_HIDDEN;

    //
    // reset message to uninitialized state
    //
    NS_HIDDEN_(void) Reset();

    // 
    // create a copy of this message
    //
    NS_HIDDEN_(ipcMessage *) Clone() const;

    //
    // initialize message
    //
    // param:
    //   topic   - message topic string
    //   data    - message data (may be null to leave data uninitialized)
    //   dataLen - message data len
    //
    NS_HIDDEN_(PRStatus) Init(const nsID &target, const char *data, PRUint32 dataLen);

    //
    // copy data into the message's data section, starting from offset.  this
    // function can be used to write any portion of the message's data.
    //
    // param:
    //   offset  - destination offset
    //   data    - data to write
    //   dataLen - number of bytes to write
    //
    NS_HIDDEN_(PRStatus) SetData(PRUint32 offset, const char *data, PRUint32 dataLen);

    //
    // access message flags
    //
    void SetFlag(PRUint16 flag)          { mMsgHdr->mFlags |= flag; }
    void ClearFlag(PRUint16 flag)        { mMsgHdr->mFlags &= ~flag; }
    PRBool TestFlag(PRUint16 flag) const { return mMsgHdr->mFlags & flag; }

    //
    // if true, the message is complete and the members of the message
    // can be accessed.
    //
    PRBool IsComplete() const { return mMsgComplete; }

    //
    // readonly accessors
    //
    const ipcMessageHeader *Header()  const { return mMsgHdr; }
    const nsID             &Target()  const { return mMsgHdr->mTarget; }
    const char             *Data()    const { return (char *) mMsgHdr + IPC_MSG_HEADER_SIZE; }
    PRUint32                DataLen() const { return mMsgHdr->mLen - IPC_MSG_HEADER_SIZE; }
    const char             *MsgBuf()  const { return (char *) mMsgHdr; }
    PRUint32                MsgLen()  const { return mMsgHdr->mLen; }

    //
    // message comparison functions
    //
    // param:
    //   topic   - message topic (may be null)
    //   data    - message data (must not be null)
    //   dataLen - message data length 
    //
    NS_HIDDEN_(PRBool) Equals(const nsID &target, const char *data, PRUint32 dataLen) const;
    NS_HIDDEN_(PRBool) Equals(const ipcMessage *msg) const;

    //
    // write the message to a buffer segment; segment need not be large
    // enough to hold entire message.  called repeatedly.
    //
    NS_HIDDEN_(PRStatus) WriteTo(char     *buf,
                                 PRUint32  bufLen,
                                 PRUint32 *bytesWritten,
                                 PRBool   *complete);

    //
    // read the message from a buffer segment; segment need not contain
    // the entire messgae.  called repeatedly.
    //
    NS_HIDDEN_(PRStatus) ReadFrom(const char *buf,
                                  PRUint32    bufLen,
                                  PRUint32   *bytesRead,
                                  PRBool     *complete);

    //
    // a message can be added to a singly-linked list.
    //
    class ipcMessage *mNext;

    //
    // meta data associated with this message object.  the owner of the
    // ipcMessage object is free to use this field for any purpose.  by
    // default, it is initialized to 0.
    //
    PRUint32          mMetaData;

private:
    ipcMessageHeader *mMsgHdr;

    // XXX document me
    PRUint32          mMsgOffset;
    PRPackedBool      mMsgComplete;
};

#endif // !ipcMessage_h__
