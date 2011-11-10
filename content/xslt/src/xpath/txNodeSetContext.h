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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
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

#ifndef __TX_XPATH_SET_CONTEXT
#define __TX_XPATH_SET_CONTEXT

#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "nsAutoPtr.h"

class txNodeSetContext : public txIEvalContext
{
public:
    txNodeSetContext(txNodeSet* aContextNodeSet, txIMatchContext* aContext)
        : mContextSet(aContextNodeSet), mPosition(0), mInner(aContext)
    {
    }

    // Iteration over the given NodeSet
    bool hasNext()
    {
        return mPosition < size();
    }
    void next()
    {
        NS_ASSERTION(mPosition < size(), "Out of bounds.");
        mPosition++;
    }
    void setPosition(PRUint32 aPosition)
    {
        NS_ASSERTION(aPosition > 0 &&
                     aPosition <= size(), "Out of bounds.");
        mPosition = aPosition;
    }

    TX_DECL_EVAL_CONTEXT;

protected:
    nsRefPtr<txNodeSet> mContextSet;
    PRUint32 mPosition;
    txIMatchContext* mInner;
};

#endif // __TX_XPATH_SET_CONTEXT
