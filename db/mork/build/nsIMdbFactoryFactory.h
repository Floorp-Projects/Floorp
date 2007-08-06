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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsIMdbFactoryFactory_h__
#define nsIMdbFactoryFactory_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

class nsIMdbFactory;

// 2794D0B7-E740-47a4-91C0-3E4FCB95B806
#define NS_IMDBFACTORYFACTORY_IID          \
{ 0x2794d0b7, 0xe740, 0x47a4, { 0x91, 0xc0, 0x3e, 0x4f, 0xcb, 0x95, 0xb8, 0x6 } }

// because Mork doesn't support XPCOM, we have to wrap the mdb factory interface
// with an interface that gives you an mdb factory.
class nsIMdbFactoryService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMDBFACTORYFACTORY_IID)
  NS_IMETHOD GetMdbFactory(nsIMdbFactory **aFactory) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIMdbFactoryService, NS_IMDBFACTORYFACTORY_IID)

#endif
