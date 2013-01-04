/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginHangUI_h
#define mozilla_plugins_PluginHangUI_h

namespace mozilla {
namespace plugins {

enum HangUIUserResponse
{
  HANGUI_USER_RESPONSE_CANCEL = 1,
  HANGUI_USER_RESPONSE_CONTINUE = 2,
  HANGUI_USER_RESPONSE_STOP = 4,
  HANGUI_USER_RESPONSE_DONT_SHOW_AGAIN = 8
};

enum PluginHangUIStructID
{
  PLUGIN_HANGUI_COMMAND = 0x10,
  PLUGIN_HANGUI_RESULT
};

struct PluginHangUICommand
{
  enum
  {
    identifier = PLUGIN_HANGUI_COMMAND
  };
  enum CmdCode
  {
    HANGUI_CMD_SHOW = 1,
    HANGUI_CMD_CANCEL = 2
  };
  CmdCode mCode;
};

struct PluginHangUIResponse
{
  enum
  {
    identifier = PLUGIN_HANGUI_RESULT
  };
  unsigned int  mResponseBits;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginHangUI_h

