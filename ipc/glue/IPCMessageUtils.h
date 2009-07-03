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
 *   Ben Turner <bent.mozilla@gmail.com>.
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

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "chrome/common/ipc_message_utils.h"

#include "prtypes.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

// Used by chrome code, we override this here and #ifdef theirs out. See the
// comments in chrome/common/ipc_message_utils.h for info about this enum.
enum IPCMessageStart {
  NPAPIProtocolMsgStart = 0,
  NPPProtocolMsgStart = 0,

  IFrameEmbeddingProtocolMsgStart,

  LastMsgIndex
};

COMPILE_ASSERT(LastMsgIndex <= 16, need_to_update_IPC_MESSAGE_MACRO);

namespace IPC {


// FIXME/cjones: PRInt16 traits had a stack corruption bug that took
// a long time to find.  putting these on ice until we need them
#if 0

template <>
struct ParamTraits<PRUint8>
{
  typedef PRUint8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(aResult),
                           sizeof(*aResult));
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%u", aParam));
  }
};

template <>
struct ParamTraits<PRInt8>
{
  typedef PRInt8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(aResult),
                           sizeof(*aResult));
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%d", aParam));
  }
};

template <>
struct ParamTraits<nsACString>
{
  typedef nsACString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType& aResult)
  {
    PRUint32 length;
    if (ReadParam(aMsg, aIter, &length)) {
      const char* buf;
      if (aMsg->ReadBytes(aIter, &buf, length)) {
        aResult.Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(UTF8ToWide(aParam.BeginReading()));
  }
};

template <>
struct ParamTraits<nsAString>
{
  typedef nsAString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(PRUnichar));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType& aResult)
  {
    PRUint32 length;
    if (ReadParam(aMsg, aIter, &length)) {
      const PRUnichar* buf;
      if (aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(&buf),
                       length * sizeof(PRUnichar))) {
        aResult.Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
#ifdef WCHAR_T_IS_UTF16
    aLog->append(reinterpret_cast<const wchar_t*>(aParam.BeginReading()));
#else
    PRUint32 length = aParam.Length();
    for (PRUint32 index = 0; index < length; index++) {
      aLog->push_back(std::wstring::value_type(aParam[index]));
    }
#endif
  }
};

template <typename E>
struct ParamTraits<nsTArray<E> >
{
  typedef nsTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    for (PRUint32 index = 0; index < length; index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType& aResult)
  {
    PRUint32 length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    // Check to make sure the message is valid before requesting a huge chunk
    // of memory.
    if (aMsg->IteratorHasRoomFor(*aIter, length * sizeof(E)) &&
        aResult.SetCapacity(length)) {
      for (PRUint32 index = 0; index < length; index++) {
        if (!ReadParam(aMsg, aIter, &aResult[index])) {
          return false;
        }
      }
    }
    else {
      // Push elements individually.
      aResult.Clear();
      E element;
      for (PRUint32 index = 0; index < length; index++) {
        if (!ReadParam(aMsg, aIter, &element) ||
            !aResult.AppendElement(element)) {
          return false;
        }
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    for (PRUint32 index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

#endif

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
