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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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

#include "nsSVGTransformList.h"
#include "nsSVGTransform.h"
#include "nsSVGMatrix.h"
#include "nsDOMError.h"
#include "prdtoa.h"
#include "nsSVGAtoms.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsCOMArray.h"

nsresult
nsSVGTransformList::Create(nsIDOMSVGTransformList** aResult)
{
  *aResult = new nsSVGTransformList();
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGTransformList::nsSVGTransformList()
{
}

nsSVGTransformList::~nsSVGTransformList()
{
  ReleaseTransforms();
}

void
nsSVGTransformList::ReleaseTransforms()
{
  PRInt32 count = mTransforms.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGTransform* transform = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(transform);
    if (val)
      val->RemoveObserver(this);
    NS_RELEASE(transform);
  }
  mTransforms.Clear();
}

nsIDOMSVGTransform*
nsSVGTransformList::ElementAt(PRInt32 index)
{
  return (nsIDOMSVGTransform*)mTransforms.ElementAt(index);
}

PRBool
nsSVGTransformList::AppendElement(nsIDOMSVGTransform* aElement)
{
  PRBool rv = mTransforms.AppendElement((void*)aElement);
  if (rv) {
    NS_ADDREF(aElement);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
    if (val)
      val->AddObserver(this);
  }

  return rv;
}

//----------------------------------------------------------------------
// XPConnect interface list
NS_CLASSINFO_MAP_BEGIN(SVGTransformList)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformList)
NS_CLASSINFO_MAP_END

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGTransformList)
NS_IMPL_RELEASE(nsSVGTransformList)

NS_INTERFACE_MAP_BEGIN(nsSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGTransformList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGTransformList::SetValueString(const nsAString& aValue)
{
  // XXX: we don't implement the _exact_ BNF given in the
  // specs. 
  
  nsresult rv = NS_OK;

  char *str = ToNewCString(aValue);
  
  char* rest = str;
  char* keyword;
  char* args;
  const char* delimiters1 = "\x20\x9\xD\xA,(";
  const char* delimiters2 = "()";
  const char* delimiters3 = "\x20\x9\xD\xA,";
  nsCOMArray<nsIDOMSVGTransform> xforms;
    
  while ((keyword = nsCRT::strtok(rest, delimiters1, &rest))) {

    while (rest && isspace(*rest))
      ++rest;

    if (!(args = nsCRT::strtok(rest, delimiters2, &rest))) {
      rv = NS_ERROR_FAILURE;
      break; // parse error
    }
    nsCOMPtr<nsIDOMSVGTransform> transform;
    NS_NewSVGTransform(getter_AddRefs(transform));
    if (!transform) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    
    nsCOMPtr<nsIAtom> keyatom = do_GetAtom(keyword);
    
    if (keyatom == nsSVGAtoms::translate) {
      char* arg1 = nsCRT::strtok(args, delimiters3, &args);
      if (!arg1) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      char* arg2 = nsCRT::strtok(args, delimiters3, &args);
      char* end;
      float tx = (float) PR_strtod(arg1, &end);
      float ty = arg2 ? (float) PR_strtod(arg2, &end) : 0.0f;
      transform->SetTranslate(tx, ty);
    }
    else if (keyatom == nsSVGAtoms::scale) { 
      char* arg1 = nsCRT::strtok(args, delimiters3, &args);
      if (!arg1) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      char* arg2 = nsCRT::strtok(args, delimiters3, &args);
      char* end;
      float sx = (float) PR_strtod(arg1, &end);
      float sy = arg2 ? (float) PR_strtod(arg2, &end) : sx;
      transform->SetScale(sx, sy);
    }      
    else if (keyatom == nsSVGAtoms::rotate) {
      char* arg1 = nsCRT::strtok(args, delimiters3, &args);
      if (!arg1) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      char* arg2 = nsCRT::strtok(args, delimiters3, &args);
      char* arg3 = arg2 ? nsCRT::strtok(args, delimiters3, &args) : nsnull;
      if (arg2 && !arg3) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      char* end;
      float angle = (float) PR_strtod(arg1, &end);
      float cx = arg2 ? (float) PR_strtod(arg2, &end) : 0.0f;
      float cy = arg3 ? (float) PR_strtod(arg3, &end) : 0.0f;
      transform->SetRotate(angle, cx, cy);
    }      
    else if (keyatom == nsSVGAtoms::skewX) {
      char* arg1 = nsCRT::strtok(args, delimiters3, &args);
      if (!arg1) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      
      char* end;
      float angle = (float) PR_strtod(arg1, &end);
      transform->SetSkewX(angle);
    }  
    else if (keyatom == nsSVGAtoms::skewY) {
      char* arg1 = nsCRT::strtok(args, delimiters3, &args);
      if (!arg1) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      char* end;
      float angle = (float) PR_strtod(arg1, &end);
      transform->SetSkewY(angle);
    }
    else if (keyatom == nsSVGAtoms::matrix) {
      char *arg, *end;

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float a = (float) PR_strtod(arg, &end);

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float b = (float) PR_strtod(arg, &end);

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float c = (float) PR_strtod(arg, &end);

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float d = (float) PR_strtod(arg, &end);

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float e = (float) PR_strtod(arg, &end);

      arg = nsCRT::strtok(args, delimiters3, &args);
      if (!arg) {
        rv = NS_ERROR_FAILURE;
        break; // parse error
      }
      float f = (float) PR_strtod(arg, &end);

      nsCOMPtr<nsIDOMSVGMatrix> matrix;
      nsSVGMatrix::Create(getter_AddRefs(matrix),
                          a, b, c, d, e, f);
      NS_ASSERTION(matrix, "couldn't create matrix");
      transform->SetMatrix(matrix);
    }
    else { // parse error
      rv = NS_ERROR_FAILURE;
      break;
    }    
    xforms.AppendObject(transform);
  }

  if (keyword || NS_FAILED(rv)) { 
    // there was a parse error. 
    rv = NS_ERROR_FAILURE;
    NS_ERROR("transform-attribute parse error");
  }
  else {
    WillModify();
    ReleaseTransforms();
    PRInt32 count = xforms.Count();
    for (PRInt32 i=0; i<count; ++i) {
      AppendElement(xforms.ObjectAt(i));
    }
    DidModify();
  }

  nsMemory::Free(str);
  
  return rv;
}

NS_IMETHODIMP
nsSVGTransformList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mTransforms.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;
  
  while (1) {
    nsIDOMSVGTransform* transform = ElementAt(i);

    nsCOMPtr<nsISVGValue> val = do_QueryInterface(transform);
    nsAutoString str;
    val->GetValueString(str);
    aValue.Append(str);

    if (++i >= count) break;

    aValue.AppendLiteral(" ");
  }
  
  return NS_OK;

}

//----------------------------------------------------------------------
// nsIDOMSVGTransformList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGTransformList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mTransforms.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGTransformList::Clear()
{
  WillModify();
  ReleaseTransforms();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGTransform initialize (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP nsSVGTransformList::Initialize(nsIDOMSVGTransform *newItem, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);
  *_retval = newItem;

  if (!newItem) return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  ReleaseTransforms();
  if (!AppendElement(newItem)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::GetItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  if ((PRInt32)index >= mTransforms.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform insertItemBefore (in nsIDOMSVGTransform newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::InsertItemBefore(nsIDOMSVGTransform *newItem, PRUint32 index, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);
  *_retval = newItem;

  if (!newItem) return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  PRInt32 count = mTransforms.Count();

  if (!mTransforms.InsertElementAt((void*)newItem, (index < count)? index: count)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(newItem);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(newItem);
  if (val)
    val->AddObserver(this);

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform replaceItem (in nsIDOMSVGTransform newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::ReplaceItem(nsIDOMSVGTransform *newItem, PRUint32 index, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);
  *_retval = nsnull;

  if (!newItem) return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  if (index >= mTransforms.Count())
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsIDOMSVGTransform* oldItem = ElementAt(index);

  if (!mTransforms.ReplaceElementAt((void*)newItem, index)) {
    NS_NOTREACHED("removal of element failed");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISVGValue> val = do_QueryInterface(oldItem);
  if (val)
    val->RemoveObserver(this);
  NS_RELEASE(oldItem);
  val = do_QueryInterface(newItem);
  if (val)
    val->AddObserver(this);
  NS_ADDREF(newItem);

  *_retval = newItem;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::RemoveItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);

  if ((PRInt32)index >= mTransforms.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval = ElementAt(index);

  if (!mTransforms.RemoveElementAt(index)) {
    NS_NOTREACHED("removal of element failed");
    *_retval = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISVGValue> val = do_QueryInterface(*_retval);
  if (val)
    val->RemoveObserver(this);

  // don't NS_ADDREF(*_retval)
  return NS_OK;
}

/* nsIDOMSVGTransform appendItem (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP nsSVGTransformList::AppendItem(nsIDOMSVGTransform *newItem, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);
  *_retval = newItem;

  if (!newItem) return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  if (!AppendElement(newItem)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform createSVGTransformFromMatrix (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGTransformList::CreateSVGTransformFromMatrix(nsIDOMSVGMatrix *matrix,
                                                 nsIDOMSVGTransform **_retval)
{
  nsresult rv = NS_NewSVGTransform(_retval);
  if (NS_FAILED(rv))
    return rv;

  (*_retval)->SetMatrix(matrix);
  return NS_OK;
}

/* nsIDOMSVGTransform consolidate (); */
NS_IMETHODIMP nsSVGTransformList::Consolidate(nsIDOMSVGTransform **_retval)
{
  // Note we don't want WillModify/DidModify since nothing really changes

  *_retval = nsnull;

  PRInt32 count = mTransforms.Count();
  if (count==0) return NS_OK;
  if (count==1) {
    *_retval = ElementAt(0);
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGMatrix> conmatrix;
  nsresult rv = GetConsolidationMatrix(getter_AddRefs(conmatrix));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMSVGTransform> consolidation;
  rv = CreateSVGTransformFromMatrix(conmatrix, getter_AddRefs(consolidation));
  if (NS_FAILED(rv))
    return rv;

  ReleaseTransforms();
  if (!AppendElement(consolidation))
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = consolidation;
  NS_ADDREF(*_retval);
  return rv;
}

/* nsIDOMSVGMatrix getConsolidation (); */
NS_IMETHODIMP nsSVGTransformList::GetConsolidationMatrix(nsIDOMSVGMatrix **_retval)
{
  *_retval = nsnull;
  PRInt32 count = mTransforms.Count();

  nsCOMPtr<nsIDOMSVGMatrix> conmatrix;
  nsresult rv = nsSVGMatrix::Create(getter_AddRefs(conmatrix));
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsIDOMSVGMatrix> temp1, temp2;

  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGTransform* transform = ElementAt(i);
    transform->GetMatrix(getter_AddRefs(temp1));
    conmatrix->Multiply(temp1, getter_AddRefs(temp2));
    if (!temp2)
      return NS_ERROR_OUT_OF_MEMORY;
    conmatrix = temp2;
  }

  *_retval = conmatrix;
  NS_ADDREF(*_retval);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGTransformList::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTransformList::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}
