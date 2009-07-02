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
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef mozilla_ipc_ProtocolUtils_h
#define mozilla_ipc_ProtocolUtils_h 1

#include "chrome/common/ipc_message_utils.h"

#include "mozilla/ipc/RPCChannel.h"

namespace mozilla {
namespace ipc {


// Used to pass references to protocol actors across the wire.  An actor
// has a parent-side "routing ID" and another routing ID on the child side.
struct ActorHandle
{
    int mParentId;
    int mChildId;
};


class /*NS_INTERFACE_CLASS*/ IProtocolManager
{
public:
    virtual int32 Register(RPCChannel::Listener*) = 0;
    RPCChannel::Listener* Lookup(int32) = 0;
    virtual void Unregister(int32) = 0;
};

} // namespace ipc
} // namespace mozilla


namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::ActorHandle>
{
  typedef mozilla::ipc::ActorHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    IPC::WriteParam(aMsg, aParam.mParentId);
    IPC::WriteParam(aMsg, aParam.mChildId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int parentId, childId;
    if (IPC::ReadParam(aMsg, aIter, &parentId)
        && ReadParam(aMsg, aIter, &childId)) {
      aResult->mParentId = parentId;
      aResult->mChildId = childId;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"(%d,%d)", aParam.mParentId, aParam.mChildId));
  }
};

} // namespace IPC


#endif  // mozilla_ipc_ProtocolUtils_h
