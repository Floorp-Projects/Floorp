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

#ifndef mozPersonalDictionary_h__
#define mozPersonalDictionary_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "mozIPersonalDictionary.h"
#include "nsIUnicodeEncoder.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

class nsAVLTree;

#define MOZ_PERSONALDICTIONARY_CONTRACTID "@mozilla.org/spellchecker/personaldictionary;1"
#define MOZ_PERSONALDICTIONARY_CID         \
{ /* FC17F1C5-367A-4d04-84A5-AF7E8F973699} */  \
0xFC17F1C5, 0x367A, 0x4d04,                    \
{ 0x84, 0xA5, 0xAF, 0x7E, 0x8F, 0x97, 0x36, 0x99} }

class mozPersonalDictionary : public mozIPersonalDictionary, 
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIPERSONALDICTIONARY
  NS_DECL_NSIOBSERVER

  mozPersonalDictionary();
  virtual ~mozPersonalDictionary();

protected:
  nsStringArray  mDictionary;  /* use something a little smarter eventually*/
  PRBool         mDirty;       /* has the dictionary been modified */
  nsString       mCharset;     /* charset that the spell checker is using */
  nsString       mLanguage;     /* the name of the language currently in use */
  nsAVLTree     *mUnicodeTree;  /* the dictionary entries */
  nsAVLTree     *mCharsetTree;  /* the dictionary entries in the current charset */
  nsAVLTree     *mUnicodeIgnoreTree;  /* the ignore all entries */
  nsAVLTree     *mCharsetIgnoreTree;  /* the ignore all entries in the current charset */
  nsCOMPtr<nsIUnicodeEncoder> mEncoder; /*Encoder to use to compare with spellchecker word */
};

#endif
