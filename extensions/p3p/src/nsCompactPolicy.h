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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#ifndef NS_COMPACTPOLICY_H__
#define NS_COMPACTPOLICY_H__

#include "nsVoidArray.h"
#include "nsHashtable.h"

#define NS_HAS_POLICY          0x00000001
#define NS_NO_POLICY           0x00000002
#define NS_INVALID_POLICY      0x00000004
#define NS_NO_CONSENT          0x00000008
#define NS_IMPLICIT_CONSENT    0x00000010
#define NS_EXPLICIT_CONSENT    0x00000020
#define NS_NON_PII_TOKEN       0x00000040
#define NS_PII_TOKEN           0x00000080

#define CP_TOKEN(_token) eToken_##_token,
enum Tokens {
  eToken_NULL,
#include "nsTokenList.h"
  eToken_last
};
#undef CP_TOKEN

#define eTokens Tokens
#define NS_CP_TOKEN_MAX PRInt32(eToken_last - 1)

class nsCompactPolicy
{
public:

  static nsresult InitTokenTable(void);
  static void DestroyTokenTable(void);

  nsCompactPolicy( );
  ~nsCompactPolicy( );

  nsresult OnHeaderAvailable(const char* aP3PHeader,
                             const char* aURI);
  nsresult GetConsent(const char* aURI,
                      PRInt32& aConsent);

protected:
  nsHashtable  mPolicyTable;
};

#endif

