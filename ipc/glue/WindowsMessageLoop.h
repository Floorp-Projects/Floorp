/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_GLUE_WINDOWSMESSAGELOOP_H
#define IPC_GLUE_WINDOWSMESSAGELOOP_H

// This file is only meant to compile on windows
#include <windows.h>

#include "nsISupportsImpl.h"

namespace mozilla {
namespace ipc {
namespace windows {

void InitUIThread();

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
