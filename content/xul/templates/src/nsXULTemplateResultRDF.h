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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef nsXULTemplateResultRDF_h__
#define nsXULTemplateResultRDF_h__

#include "nsCOMPtr.h"
#include "nsIRDFResource.h"
#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsRDFQuery.h"
#include "nsRuleNetwork.h"
#include "nsIXULTemplateResult.h"
#include "nsRDFBinding.h"

/**
 * A single result of a query on an RDF graph
 */
class nsXULTemplateResultRDF : public nsIXULTemplateResult
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsXULTemplateResultRDF)

    NS_DECL_NSIXULTEMPLATERESULT

    nsXULTemplateResultRDF(nsIRDFResource* aNode);

    nsXULTemplateResultRDF(nsRDFQuery* aQuery,
                           const Instantiation& aInst,
                           nsIRDFResource* aNode);

    ~nsXULTemplateResultRDF();

    nsITemplateRDFQuery* Query() { return mQuery; }

    nsXULTemplateQueryProcessorRDF* GetProcessor()
    {
        return (mQuery ? mQuery->Processor() : nsnull);
    }

    /**
     * Get the value of a variable, first by looking in the assignments and
     * then the bindings
     */
    void
    GetAssignment(nsIAtom* aVar, nsIRDFNode** aValue);

    /**
     * Synchronize the bindings after a change in the RDF graph. Bindings that
     * would be affected will be assigned appropriately based on the change.
     */
    PRBool
    SyncAssignments(nsIRDFResource* aSubject,
                    nsIRDFResource* aPredicate,
                    nsIRDFNode* aTarget);

    /**
     * Return true if the result has an instantiation involving a particular
     * memory element.
     */
    PRBool
    HasMemoryElement(const MemoryElement& aMemoryElement);

protected:

    // query that generated the result
    nsCOMPtr<nsITemplateRDFQuery> mQuery;

    // resource node
    nsCOMPtr<nsIRDFResource> mNode;

    // data computed from query
    Instantiation mInst;

    // extra assignments made by rules (<binding> tags)
    nsBindingValues mBindingValues;
};

#endif // nsXULTemplateResultRDF_h__
