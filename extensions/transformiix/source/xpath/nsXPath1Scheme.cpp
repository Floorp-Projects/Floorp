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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Heikki Toivonen <heikki@netscape.com> (Original Author)
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

/**
 * This file implements the xpath1 XPointer scheme, and the xmlns scheme
 * as well but only for xpath1.
 *
 * http://www.simonstl.com/ietf/draft-stlaurent-xpath-frag-00.html
 */

#include "nsXPath1Scheme.h"
#include "nsXPathEvaluator.h"
#include "nsXPathException.h"
#include "nsDOMError.h"
#include "nsXPathResult.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMRange.h"
#include "nsDOMString.h"
#include "nsIModifyableXPointer.h"
#include "nsAutoPtr.h"
#include "nsString.h"

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

/**
 * nsXPath1SchemeNSResolver
 *
 * This will effectively give us xmlns scheme support.
 */
class nsXPath1SchemeNSResolver : public nsIDOMXPathNSResolver
{
public:
  nsXPath1SchemeNSResolver(nsIXPointerSchemeContext *aContext)
    : mContext(aContext)
  {
  }
  
  virtual ~nsXPath1SchemeNSResolver()
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMXPATHNSRESOLVER

private:
  nsCOMPtr<nsIXPointerSchemeContext> mContext;
};

NS_IMPL_ISUPPORTS1(nsXPath1SchemeNSResolver, nsIDOMXPathNSResolver)

//DOMString          lookupNamespaceURI(in DOMString prefix);
NS_IMETHODIMP
nsXPath1SchemeNSResolver::LookupNamespaceURI(const nsAString &aPrefix,
                                             nsAString &aURI)
{
  aURI.Truncate();

  // This method will be called each time the XPath engine encounters
  // a prefix.
  
  // We could cache the extracted prefix + URI pairs in a hash table,
  // and do a lookup from that first. But typical XPointers only have
  // a few scheme + data pairs (and only some of those will be xmlns
  // schemes), and typical XPath expressions only have a few prefixes
  // as well, so we'll see if we can manage without...

  if (!mContext) {
    return NS_OK;
  }

  NS_NAMED_LITERAL_STRING(xmlns, "xmlns");

  PRUint32 count;
  mContext->GetCount(&count);
  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsAutoString scheme, data;
    mContext->GetSchemeData(i, scheme, data);
    if (scheme.Equals(xmlns)) {
      PRInt32 sep = data.FindChar('=');
      if (sep > 0 && aPrefix.Equals(Substring(data, 0, sep))) {
        aURI.Assign(Substring(data, sep + 1, data.Length() - sep - 1));
        return NS_OK;
      }
    }
  }

  SetDOMStringToNull(aURI);

  return NS_OK;
}

// nsXPath1SchemeProcessor
nsXPath1SchemeProcessor::nsXPath1SchemeProcessor()
{
}

nsXPath1SchemeProcessor::~nsXPath1SchemeProcessor()
{
}

NS_IMPL_ISUPPORTS1(nsXPath1SchemeProcessor, nsIXPointerSchemeProcessor)

/**
 * Evaluate.
 *
 * @param aDocument The document in which to resolve the XPointer.
 * @param aContext  The XPointer context in which to process aData.
 * @param aData     The data in the scheme that needs to be resolved.
 * @return          The result of the evaluation.
 */
NS_IMETHODIMP
nsXPath1SchemeProcessor::Evaluate(nsIDOMDocument *aDocument,
                         nsIXPointerSchemeContext *aContext,
                         const nsAString &aData,
                         nsIXPointerResult **aResult)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aContext);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
 
  // Resolve expression
  nsCOMPtr<nsIDOMXPathNSResolver> nsresolver(new nsXPath1SchemeNSResolver(aContext));
  if (!nsresolver) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsRefPtr<nsXPathEvaluator> e(new nsXPathEvaluator(nsnull));
  if (!e) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = e->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXPathResult> result;
  rv = e->Evaluate(aData,
                   aDocument,
                   nsresolver,
                   nsIDOMXPathResult::ORDERED_NODE_ITERATOR_TYPE,
                   nsnull,
                   getter_AddRefs(result));
  if (NS_FAILED(rv)) {
    if ((rv == NS_ERROR_DOM_INVALID_EXPRESSION_ERR) ||
        (rv == NS_ERROR_DOM_NAMESPACE_ERR) ||
        (rv == NS_ERROR_DOM_TYPE_ERR)) {
      // These errors are benign, change them to NS_OK so that
      // we will not terminate the processor.
      rv = NS_OK;
    }
    return rv;
  }

  // Create return result
  // XXX perf: just store the XPathResult and resolve as XPointerResult on demand
  nsCOMPtr<nsIXPointerResult> xpointerResult(
    do_CreateInstance("@mozilla.org/xmlextras/xpointerresult;1", &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIModifyableXPointerResult> privatePointerResult(do_QueryInterface(xpointerResult));
  if (!privatePointerResult) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> node;
  rv = result->IterateNext(getter_AddRefs(node));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Fill in return result
  while (node) {
    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID, &rv));
    if (NS_FAILED(rv))
      break;

    rv = range->SelectNode(node);
    if (NS_FAILED(rv))
      break;

    rv = privatePointerResult->AppendRange(range);
    if (NS_FAILED(rv))
      break;

    rv = result->IterateNext(getter_AddRefs(node));
    if (NS_FAILED(rv))
      break;
  }

  PRUint32 count;
  xpointerResult->GetLength(&count);
  if (NS_SUCCEEDED(rv) && (count > 0)) {
    *aResult = xpointerResult;
    NS_ADDREF(*aResult);
  }

  return rv;
}
