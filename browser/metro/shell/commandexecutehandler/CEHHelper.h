/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x602
#define _WIN32_WINNT 0x602

#include <windows.h>
#include <d3d10_1.h>
#include <dxgi.h>
#include <d3d10misc.h>
#include <atlbase.h>

//#define SHOW_CONSOLE 1
extern HANDLE sCon;
extern LPCWSTR metroDX10Available;

void Log(const wchar_t *fmt, ...);

#if defined(SHOW_CONSOLE)
void SetupConsole();
#endif

bool IsDX10Available();
bool GetDWORDRegKey(LPCWSTR name, DWORD &value);
bool SetDWORDRegKey(LPCWSTR name, DWORD value);
