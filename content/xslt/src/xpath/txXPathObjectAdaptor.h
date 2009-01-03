/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 20008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Merle Sterling <msterlin@us.ibm.com>

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

#ifndef txXPathObjectAdaptor_h__
#define txXPathObjectAdaptor_h__

#include "txExprResult.h"
#include "txINodeSet.h"
#include "txIXPathObject.h"

/**
 * Implements an XPCOM wrapper around XPath data types boolean, number, string,
 * or nodeset.
 */

class txXPathObjectAdaptor : public txIXPathObject
{
public:
    txXPathObjectAdaptor(txAExprResult *aValue) : mValue(aValue)
    {
        NS_ASSERTION(aValue,
                     "Don't create a txXPathObjectAdaptor if you don't have a "
                     "txAExprResult");
    }

    NS_DECL_ISUPPORTS

    NS_IMETHODIMP_(txAExprResult*) GetResult()
    {
        return mValue;
    }

protected:
    txXPathObjectAdaptor() : mValue(nsnull)
    {
    }

    nsRefPtr<txAExprResult> mValue;
};

#endif // txXPathObjectAdaptor_h__
