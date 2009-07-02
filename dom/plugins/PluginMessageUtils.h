/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

namespace IPC {

#if 0
  void* window;  /* Platform specific window handle */
                 /* OS/2: x - Position of bottom left corner */
                 /* OS/2: y - relative to visible netscape window */
  int32_t  x;      /* Position of top left corner relative */
  int32_t  y;      /* to a netscape page. */
  uint32_t width;  /* Maximum window size */
  uint32_t height;
  NPRect   clipRect; /* Clipping rectangle in port coordinates */
                     /* Used by MAC only. */
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  void * ws_info; /* Platform-dependent additonal data */
#endif /* XP_UNIX */
  NPWindowType type; /* Is this a window or a drawable? */
#endif

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

} /* namespace IPC */

#endif /* DOM_PLUGINS_PLUGINMESSAGEUTILS_H */
