/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _AtkSocketAccessible_H_
#define _AtkSocketAccessible_H_

#include "AccessibleWrap.h"

// This file gets included by nsAccessibilityService.cpp, which can't include
// atk.h (or glib.h), so we can't rely on it being included.
#ifdef __ATK_H__
extern "C" typedef void (*AtkSocketEmbedType) (AtkSocket*, gchar*);
#else
extern "C" typedef void (*AtkSocketEmbedType) (void*, void*);
#endif

namespace mozilla {
namespace a11y {

/**
 * Provides a AccessibleWrap wrapper around AtkSocket for out-of-process
 * accessibles.
 */
class AtkSocketAccessible : public AccessibleWrap
{
public:

  // Soft references to AtkSocket
  static AtkSocketEmbedType g_atk_socket_embed;
#ifdef __ATK_H__
  static GType g_atk_socket_type;
#endif
  static const char* sATKSocketEmbedSymbol;
  static const char* sATKSocketGetTypeSymbol;

  /*
   * True if the current Atk version supports AtkSocket and it was correctly
   * loaded.
   */
  static bool gCanEmbed;

  AtkSocketAccessible(nsIContent* aContent, DocAccessible* aDoc,
                      const nsCString& aPlugId);

  virtual void Shutdown();

  virtual void GetNativeInterface(void** aOutAccessible) MOZ_OVERRIDE;
};

} // namespace a11y
} // namespace mozilla

#endif
