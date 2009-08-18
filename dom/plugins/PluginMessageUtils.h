/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Plugin App.
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

#ifndef DOM_PLUGINS_PLUGINMESSAGEUTILS_H
#define DOM_PLUGINS_PLUGINMESSAGEUTILS_H

#include "IPC/IPCMessageUtils.h"

#include "npapi.h"
#include "npruntime.h"
#include "nsAutoPtr.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace ipc {

typedef intptr_t NPRemoteIdentifier;

} /* namespace ipc */

namespace plugins {

/**
 * This is NPByteRange without the linked list.
 */
struct IPCByteRange
{
  int32_t offset;
  uint32_t length;
};  

typedef std::vector<IPCByteRange> IPCByteRanges;

typedef nsCString Buffer;

} /* namespace plugins */

} /* namespace mozilla */

namespace IPC {

template <>
struct ParamTraits<NPRect>
{
  typedef NPRect paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.top);
    WriteParam(aMsg, aParam.left);
    WriteParam(aMsg, aParam.bottom);
    WriteParam(aMsg, aParam.right);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint16_t top, left, bottom, right;
    if (ReadParam(aMsg, aIter, &top) &&
        ReadParam(aMsg, aIter, &left) &&
        ReadParam(aMsg, aIter, &bottom) &&
        ReadParam(aMsg, aIter, &right)) {
      aResult->top = top;
      aResult->left = left;
      aResult->bottom = bottom;
      aResult->right = right;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %u, %u, %u]", aParam.top, aParam.left,
                              aParam.bottom, aParam.right));
  }
};

template <>
struct ParamTraits<NPWindowType>
{
  typedef NPWindowType paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteInt16(int16(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int16 result;
    if (aMsg->ReadInt16(aIter, &result)) {
      *aResult = paramType(result);
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%d", int16(aParam)));
  }
};

template <>
struct ParamTraits<NPWindow>
{
  typedef NPWindow paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteULong(reinterpret_cast<unsigned long>(aParam.window));
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
    WriteParam(aMsg, aParam.clipRect);
    // we don't serialize ws_info because it stores pointers to this
    // process's address space.  it is reconstructed for each process
    // using the window ID
    WriteParam(aMsg, aParam.type);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    unsigned long window;
    int32_t x, y;
    uint32_t width, height;
    NPWindowType type;
    if (aMsg->ReadULong(aIter, &window) &&
        ReadParam(aMsg, aIter, &x) &&
        ReadParam(aMsg, aIter, &y) &&
        ReadParam(aMsg, aIter, &width) &&
        ReadParam(aMsg, aIter, &height) &&
        ReadParam(aMsg, aIter, &type)) {
      aResult->window = (void*)window;
      aResult->x = x;
      aResult->y = y;
      aResult->width = width;
      aResult->height = height;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
      aResult->ws_info = 0;     // graphics code fills this in
#endif
      aResult->type = type;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %d, %d, %u, %u, %d",
                              (unsigned long)aParam.window,
                              aParam.x, aParam.y, aParam.width,
                              aParam.height, (long)aParam.type));
  }
};

template <>
struct ParamTraits<NPString>
{
  typedef NPString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.UTF8Length);
    aMsg->WriteBytes(aParam.UTF8Characters,
                     aParam.UTF8Length * sizeof(NPUTF8));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->UTF8Length)) {
      int byteCount = aResult->UTF8Length * sizeof(NPUTF8);
      if (!byteCount) {
        aResult->UTF8Characters = "\0";
        return true;
      }

      const char* messageBuffer = nsnull;
      nsAutoArrayPtr<char> newBuffer(new char[byteCount]);
      if (newBuffer && aMsg->ReadBytes(aIter, &messageBuffer, byteCount )) {
        memcpy((void*)messageBuffer, newBuffer.get(), byteCount);
        aResult->UTF8Characters = newBuffer.forget();
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%s", aParam.UTF8Characters));
  }
};

template <>
struct ParamTraits<NPVariant>
{
  typedef NPVariant paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    if (NPVARIANT_IS_VOID(aParam)) {
      aMsg->WriteInt(0);
      return;
    }

    if (NPVARIANT_IS_NULL(aParam)) {
      aMsg->WriteInt(1);
      return;
    }

    if (NPVARIANT_IS_BOOLEAN(aParam)) {
      aMsg->WriteInt(2);
      WriteParam(aMsg, NPVARIANT_TO_BOOLEAN(aParam));
      return;
    }

    if (NPVARIANT_IS_INT32(aParam)) {
      aMsg->WriteInt(3);
      WriteParam(aMsg, NPVARIANT_TO_INT32(aParam));
      return;
    }

    if (NPVARIANT_IS_DOUBLE(aParam)) {
      aMsg->WriteInt(4);
      WriteParam(aMsg, NPVARIANT_TO_DOUBLE(aParam));
      return;
    }

    if (NPVARIANT_IS_STRING(aParam)) {
      aMsg->WriteInt(5);
      WriteParam(aMsg, NPVARIANT_TO_STRING(aParam));
      return;
    }

    NS_ERROR("Unsupported type!");
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int type;
    if (!aMsg->ReadInt(aIter, &type)) {
      return false;
    }

    switch (type) {
      case 0:
        VOID_TO_NPVARIANT(*aResult);
        return true;

      case 1:
        NULL_TO_NPVARIANT(*aResult);
        return true;

      case 2: {
        bool value;
        if (ReadParam(aMsg, aIter, &value)) {
          BOOLEAN_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 3: {
        int32 value;
        if (ReadParam(aMsg, aIter, &value)) {
          INT32_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 4: {
        double value;
        if (ReadParam(aMsg, aIter, &value)) {
          DOUBLE_TO_NPVARIANT(value, *aResult);
          return true;
        }
      } break;

      case 5: {
        NPString value;
        if (ReadParam(aMsg, aIter, &value)) {
          STRINGN_TO_NPVARIANT(value.UTF8Characters, value.UTF8Length,
                               *aResult);
          return true;
        }
      } break;

      default:
        NS_ERROR("Unsupported type!");
    }

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (NPVARIANT_IS_VOID(aParam)) {
      aLog->append(L"[void]");
      return;
    }

    if (NPVARIANT_IS_NULL(aParam)) {
      aLog->append(L"[null]");
      return;
    }

    if (NPVARIANT_IS_BOOLEAN(aParam)) {
      LogParam(NPVARIANT_TO_BOOLEAN(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_INT32(aParam)) {
      LogParam(NPVARIANT_TO_INT32(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_DOUBLE(aParam)) {
      LogParam(NPVARIANT_TO_DOUBLE(aParam), aLog);
      return;
    }

    if (NPVARIANT_IS_STRING(aParam)) {
      LogParam(NPVARIANT_TO_STRING(aParam), aLog);
      return;
    }

    NS_ERROR("Unsupported type!");
  }
};

template <>
struct ParamTraits<mozilla::plugins::IPCByteRange>
{
  typedef mozilla::plugins::IPCByteRange paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.offset);
    WriteParam(aMsg, aParam.length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    paramType p;
    if (ReadParam(aMsg, aIter, &p.offset) &&
        ReadParam(aMsg, aIter, &p.length)) {
      *aResult = p;
      return true;
    }
    return false;
  }
};

} /* namespace IPC */

#endif /* DOM_PLUGINS_PLUGINMESSAGEUTILS_H */
