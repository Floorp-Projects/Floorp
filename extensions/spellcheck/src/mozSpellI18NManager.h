/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein Deinst@world.std.com
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

#ifndef mozSpellI18NManager_h__
#define mozSpellI18NManager_h__

#include "nsCOMPtr.h"
#include "mozISpellI18NManager.h"

#define MOZ_SPELLI18NMANAGER_CONTRACTID "@mozilla.org/spellchecker/i18nmanager;1"
#define MOZ_SPELLI18NMANAGER_CID         \
{ /* {F30F3CC0-840F-4a83-AC0E-FD343D3A44A0} */  \
0xf30f3cc0, 0x840f, 0x4a83,                    \
{ 0xac, 0xe, 0xfd, 0x34, 0x3d, 0x3a, 0x44, 0xa0} }


class mozSpellI18NManager : public mozISpellI18NManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISPELLI18NMANAGER

  mozSpellI18NManager();
  virtual ~mozSpellI18NManager();
};
#endif

