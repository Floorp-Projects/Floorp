/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Erik van der Poel
 *   Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsILanguageAtomService_h_
#define nsILanguageAtomService_h_

/*
 * The nsILanguageAtomService provides a mapping from languages or
 * character sets to language groups.
 */

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

#define NS_ILANGUAGEATOMSERVICE_IID \
  {0x24b45737, 0x9e94, 0x4e40, \
    { 0x9d, 0x59, 0x29, 0xcd, 0x62, 0x96, 0x3a, 0xdd }}

#define NS_LANGUAGEATOMSERVICE_CONTRACTID \
  "@mozilla.org/intl/nslanguageatomservice;1"

class nsILanguageAtomService : public nsISupports
{
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILANGUAGEATOMSERVICE_IID)

  virtual nsIAtom* LookupLanguage(const nsAString &aLanguage,
                                  nsresult *aError = nsnull) = 0;
  virtual already_AddRefed<nsIAtom>
  LookupCharSet(const char *aCharSet, nsresult *aError = nsnull) = 0;

  virtual nsIAtom* GetLocaleLanguageGroup(nsresult *aError = nsnull) = 0;
};

#endif
