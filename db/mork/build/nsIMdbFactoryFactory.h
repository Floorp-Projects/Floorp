/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsIMdbFactoryFactory_h__
#define nsIMdbFactoryFactory_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

class nsIMdbFactory;

// d5440650-2807-11d3-8d75-00805f8a6617
#define NS_IMDBFACTORYFACTORY_IID          \
{ 0xd5440650, 0x2807, 0x11d3, { 0x8d, 0x75, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17}}

// because Mork doesn't support XPCOM, we have to wrap the mdb factory interface
// with an interface that gives you an mdb factory.
class nsIMdbFactoryFactory : public nsISupports
{
public:
    static const nsIID& GetIID(void) { static nsIID iid = NS_IMDBFACTORYFACTORY_IID; return iid; }
	NS_IMETHOD GetMdbFactory(nsIMdbFactory **aFactory) = 0;
};

#endif
