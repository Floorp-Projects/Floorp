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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsLSSerializer_h__
#define nsLSSerializer_h__

#include "nsIDOMLSSerializer.h"
#include "nsIDOMSerializer.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIDOMLSSerializerFilter;


#define NS_LSSERIALIZER_CID                           \
  { /* 17a4357c-f052-47a1-8e1d-37fe22b94a61 */        \
    0x17a4357c, 0xf052, 0x47a1,                       \
   {0x8e, 0x1d, 0x37, 0xfe, 0x22, 0xb9, 0x4a, 0x61} }

#define NS_LSSERIALIZER_CONTRACTID \
  "@mozilla.org/dom/lsserializer;1"


class nsLSSerializer : public nsIDOMLSSerializer
{
public:
  nsLSSerializer();
  virtual ~nsLSSerializer();

  NS_DECL_ISUPPORTS

  // nsIDOMLSSerializer
  NS_DECL_NSIDOMLSSERIALIZER

  nsresult Init();

protected:
  nsCOMPtr<nsIDOMSerializer> mInnerSerializer;
  nsCOMPtr<nsIDOMLSSerializerFilter> mFilter;

  nsCString mNewLine;
};



#endif

