/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 */
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef IPC_GLUE_WINDOWSMESSAGELOOP_H
#define IPC_GLUE_WINDOWSMESSAGELOOP_H

// This file is only meant to compile on windows
#include <windows.h>

#include "base/basictypes.h"
#include "nsTraceRefcnt.h"

namespace mozilla {
namespace ipc {
namespace windows {

class DeferredMessage
{
public:
  DeferredMessage()
  {
    MOZ_COUNT_CTOR(DeferredMessage);
  }

  virtual ~DeferredMessage()
  {
    MOZ_COUNT_DTOR(DeferredMessage);
  }

  virtual void Run() = 0;
};

// Uses CallWndProc to deliver a message at a later time. Messages faked with
// this class must not have pointers in their wParam or lParam members as they
// may be invalid by the time the message actually runs.
class DeferredSendMessage : public DeferredMessage
{
public:
  DeferredSendMessage(HWND aHWnd,
                      UINT aMessage,
                      WPARAM aWParam,
                      LPARAM aLParam)
    : hWnd(aHWnd),
      message(aMessage),
      wParam(aWParam),
      lParam(aLParam)
  { }

  virtual void Run();

protected:
    HWND hWnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};

// Uses RedrawWindow to fake several painting-related messages. Flags passed
// to the constructor go directly to RedrawWindow.
class DeferredRedrawMessage : public DeferredMessage
{
public:
  DeferredRedrawMessage(HWND aHWnd,
                        UINT aFlags)
    : hWnd(aHWnd),
      flags(aFlags)
  { }

  virtual void Run();

private:
  HWND hWnd;
  UINT flags;
};

// Uses UpdateWindow to generate a WM_PAINT message if needed.
class DeferredUpdateMessage : public DeferredMessage
{
public:
  DeferredUpdateMessage(HWND aHWnd);

  virtual void Run();

private:
  HWND mWnd;
  RECT mUpdateRect;
};

// This class duplicates a string that may exist in the lParam member of the
// message.
class DeferredSettingChangeMessage : public DeferredSendMessage
{
public:
  DeferredSettingChangeMessage(HWND aHWnd,
                               UINT aMessage,
                               WPARAM aWParam,
                               LPARAM aLParam);

  ~DeferredSettingChangeMessage();
private:
  wchar_t* lParamString;
};

// This class uses SetWindowPos to fake various size-related messages. Flags
// passed to the constructor go straight through to SetWindowPos.
class DeferredWindowPosMessage : public DeferredMessage
{
public:
  DeferredWindowPosMessage(HWND aHWnd,
                           LPARAM aLParam,
                           bool aForCalcSize = false,
                           WPARAM aWParam = 0);

  virtual void Run();

private:
  WINDOWPOS windowPos;
};

// This class duplicates a data buffer for a WM_COPYDATA message.
class DeferredCopyDataMessage : public DeferredSendMessage
{
public:
  DeferredCopyDataMessage(HWND aHWnd,
                          UINT aMessage,
                          WPARAM aWParam,
                          LPARAM aLParam);

  ~DeferredCopyDataMessage();
private:
  COPYDATASTRUCT copyData;
};

class DeferredStyleChangeMessage : public DeferredMessage
{
public:
  DeferredStyleChangeMessage(HWND aHWnd,
                             WPARAM aWParam,
                             LPARAM aLParam);

  virtual void Run();

private:
  HWND hWnd;
  int index;
  LONG_PTR style;
};

class DeferredSetIconMessage : public DeferredSendMessage
{
public:
  DeferredSetIconMessage(HWND aHWnd,
                         UINT aMessage,
                         WPARAM aWParam,
                         LPARAM aLParam);

  virtual void Run();
};

} /* namespace windows */
} /* namespace ipc */
} /* namespace mozilla */

#endif /* IPC_GLUE_WINDOWSMESSAGELOOP_H */
