/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*/
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Heikki Toivonen <heikki@netscape.com> (original author)
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
 * Implementation for the XPointer family of specifications, practically the
 * XPointer Processor. The processor can call optional modules that implement
 * some XPointer schemes that were not implemented in this file. Please note
 * that implementation of the xmlns scheme is left to the optional scheme
 * implementations - all the information they need will be passed in.
 *
 * The framework:
 *   http://www.w3.org/TR/xptr-framework/
 * The element scheme:
 *   http://www.w3.org/TR/xptr-element/
 *
 * Additionally this module implements 'fixptr' scheme for the FIXptr
 * W3C proposal:
 *   http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
 */

// TODO:
// - xpointer scheme


#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMClassInfo.h"
#include "nsCOMPtr.h"
#include "nsXPointer.h"
#include "nsIModifyableXPointer.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsIXPointer.h"
#include "nsFIXptr.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

nsXPointerResult::nsXPointerResult()
{
}

nsXPointerResult::~nsXPointerResult()
{
}

NS_INTERFACE_MAP_BEGIN(nsXPointerResult)
  NS_INTERFACE_MAP_ENTRY(nsIXPointerResult)
  NS_INTERFACE_MAP_ENTRY(nsIModifyableXPointerResult)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPointerResult)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXPointerResult)
NS_IMPL_RELEASE(nsXPointerResult)

NS_IMETHODIMP
nsXPointerResult::AppendRange(nsIDOMRange* aRange)
{
  NS_ENSURE_ARG(aRange);
  
  if (!mArray.AppendObject(aRange)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPointerResult::Item(PRUint32 aIndex, nsIDOMRange** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  if (aIndex >= (PRUint32)mArray.Count()) {
    return NS_ERROR_FAILURE;
  }

  *aReturn = mArray.ObjectAt(aIndex);
  NS_IF_ADDREF(*aReturn);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXPointerResult::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = mArray.Count();

  return NS_OK;
}

nsresult NS_NewXPointerResult(nsIXPointerResult **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = new nsXPointerResult();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(*aResult);

  return NS_OK;
}

static nsresult NS_NewXPointerResult(nsIDOMRange *aRange,
                                     nsIXPointerResult **aResult)
{
  NS_ENSURE_ARG(aRange);
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsXPointerResult> result(new nsXPointerResult());
  if (!result) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult rv = result->AppendRange(aRange);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = result.get();
  NS_ADDREF(*aResult);

  return NS_OK;
}

static nsresult NS_NewXPointerResult(nsIDOMNode *aNode,
                                     nsIXPointerResult **aResult)
{
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (!range) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = range->SelectNode(aNode);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_NewXPointerResult(range, aResult);
}


// nsXPointerSchemeContext

class nsXPointerSchemeContext : public nsIXPointerSchemeContext
{
public:
  nsXPointerSchemeContext() {};
  virtual ~nsXPointerSchemeContext() {};
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPOINTERSCHEMECONTEXT

  nsresult Append(const nsAString &aScheme, const nsAString &aData);

private:
  nsStringArray mSchemes;
  nsStringArray mDatas;
};

NS_IMPL_ISUPPORTS1(nsXPointerSchemeContext, nsIXPointerSchemeContext)

NS_IMETHODIMP
nsXPointerSchemeContext::GetCount(PRUint32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  *aCount = mSchemes.Count();

  return NS_OK;
}


nsresult
nsXPointerSchemeContext::Append(const nsAString &aScheme,
                                const nsAString &aData)
{
  if (!mSchemes.AppendString(aScheme)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mDatas.AppendString(aData)) {
    // Keep mDatas and mSchemes in sync
    mSchemes.RemoveStringAt(mSchemes.Count() - 1);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPointerSchemeContext::GetSchemeData(PRUint32 aIndex,
                                       nsAString &aScheme,
                                       nsAString &aData)
{
  if (aIndex >= (PRUint32)mSchemes.Count()) {
    aScheme.Truncate();
    aData.Truncate();

    return NS_ERROR_FAILURE;
  }

  mSchemes.StringAt(aIndex, aScheme);
  mDatas.StringAt(aIndex, aData);

  return NS_OK;
}

// XPointer

nsXPointer::nsXPointer()
{
}

nsXPointer::~nsXPointer()
{
}

NS_IMPL_ISUPPORTS1(nsXPointer, nsIXPointerEvaluator)

static nsresult GetNextSchemeNameAndData(nsString& aExpression,
                                         nsString &aScheme,
                                         nsString& aData)
{
  aScheme.Truncate();
  aData.Truncate();

  PRInt32 lp = aExpression.FindChar('(');
  if (lp < 1) {
    return NS_ERROR_FAILURE; // format |scheme + '(' [ + data + ] + ')'| required
  }

  PRInt32 i = lp + 1, len = aExpression.Length();
  if (i >= len) {
    return NS_ERROR_FAILURE; // format |scheme + '(' [ + data + ] + ')'| required
  }

  aScheme = Substring(aExpression, 0, lp);
  aScheme.CompressWhitespace(PR_TRUE, PR_FALSE);
  if (aScheme.FindCharInSet(" \t\r\n") > 0) {
    return NS_ERROR_FAILURE; // scheme name can't contain ws (we'd really need to check a lot more...)
  }

  // XXX perf: Switch to string iterators
  PRBool escapeOn = PR_FALSE;
  PRInt32 balance = 1;
  for (; i < len; ++i) {
    // Circumflex is the escape character that can precede ^, ( and ) only
    if (aExpression[i] == '^') {
      if (!escapeOn) {
        escapeOn = PR_TRUE;
        continue;
      }
    } else if (escapeOn) {
      if ((aExpression[i] != '(') && (aExpression[i] != ')')) {
        return NS_ERROR_FAILURE; // illegal use of ^
      }
    } else if (aExpression[i] == '(') {
      ++balance;
    } else if (aExpression[i] == ')') {
      if (--balance == 0) {
        aExpression.Cut(0, i + 1);
        break;
      }
    }

    aData.Append(aExpression[i]);
    escapeOn = PR_FALSE;
  }

  if (balance != 0) {
    return NS_ERROR_FAILURE; // format |scheme + '(' [ + data + ] + ')'| required
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPointer::Evaluate(nsIDOMDocument *aDocument,
                     const nsAString& aExpression,
                     nsIXPointerResult **aResult)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  nsresult rv = NS_OK;

  if (aExpression.FindChar('(') < 0) {
    // Must be shorthand, i.e. plain id
    nsCOMPtr<nsIDOMElement> element;
    aDocument->GetElementById(aExpression, getter_AddRefs(element));
    if (element) {
      rv = NS_NewXPointerResult(element, aResult);
    }
    return rv;
  }

  nsAutoString expression(aExpression), scheme, data;

  NS_NAMED_LITERAL_STRING(element, "element");
  NS_NAMED_LITERAL_STRING(fixptr, "fixptr");
  NS_NAMED_LITERAL_CSTRING(baseSchemeProgID, NS_XPOINTER_SCHEME_PROCESSOR_BASE);
  nsCOMPtr<nsXPointerSchemeContext> contextSchemeDataArray(new nsXPointerSchemeContext());
  if (!contextSchemeDataArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Keep trying the schemes from left to right until one finds a subresource
  while (!expression.IsEmpty()) {
    rv = GetNextSchemeNameAndData(expression, scheme, data);
    if (NS_FAILED(rv))
      break;

    // Built in schemes
    if (scheme.Equals(element)) {
      // We implement element scheme by using the FIXptr processor.
      // Check there are no parenthesis (legal in FIXptr data).
      if (data.FindChar('(') < 0) {
        nsCOMPtr<nsIDOMRange> range;
        nsCOMPtr<nsIFIXptrEvaluator> e(new nsFIXptr());
        if (!e)
          return NS_ERROR_OUT_OF_MEMORY;
        rv = e->Evaluate(aDocument, data, getter_AddRefs(range));
        if (NS_FAILED(rv))
          break;
        if (range) {
          return NS_NewXPointerResult(range, aResult);
        }
      }
    } else if (scheme.Equals(fixptr)) {
      nsCOMPtr<nsIDOMRange> range;
      nsCOMPtr<nsIFIXptrEvaluator> e(new nsFIXptr());
      if (!e)
        return NS_ERROR_OUT_OF_MEMORY;
      rv = e->Evaluate(aDocument, data, getter_AddRefs(range));
      if (NS_FAILED(rv))
        break;
      if (range) {
        return NS_NewXPointerResult(range, aResult);
      }
    } else {
      // Add-on schemes
      nsCAutoString progid(baseSchemeProgID);
      AppendUTF16toUTF8(scheme, progid);
      nsCOMPtr<nsIXPointerSchemeProcessor> p(do_CreateInstance(progid.get()));
      if (p) {
        rv = p->Evaluate(aDocument, contextSchemeDataArray, data, aResult);
        if (NS_FAILED(rv))
          break;
        if (*aResult)
          return NS_OK;
      }
    }

    rv = contextSchemeDataArray->Append(scheme, data);
    if (NS_FAILED(rv))
      break;

  }

  return rv;
}

