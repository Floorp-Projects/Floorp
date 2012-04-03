/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Taylor <brad@getcoded.net> (original author)
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

#ifndef _AtkSocketAccessible_H_
#define _AtkSocketAccessible_H_

#include "nsAccessibleWrap.h"

// This file gets included by nsAccessibilityService.cpp, which can't include
// atk.h (or glib.h), so we can't rely on it being included.
#ifdef __ATK_H__
extern "C" typedef void (*AtkSocketEmbedType) (AtkSocket*, gchar*);
#else
extern "C" typedef void (*AtkSocketEmbedType) (void*, void*);
#endif

/**
 * Provides a nsAccessibleWrap wrapper around AtkSocket for out-of-process
 * accessibles.
 */
class AtkSocketAccessible: public nsAccessibleWrap
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

  AtkSocketAccessible(nsIContent* aContent, nsDocAccessible* aDoc,
                      const nsCString& aPlugId);

  // nsAccessNode
  virtual void Shutdown();

  // nsIAccessible
  NS_IMETHODIMP GetNativeInterface(void** aOutAccessible);
};

#endif
