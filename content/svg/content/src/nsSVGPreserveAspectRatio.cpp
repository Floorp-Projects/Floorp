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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is
 * Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jonathan.watt@strath.ac.uk> (original author)
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

#include "nsSVGPreserveAspectRatio.h"
#include "nsSVGValue.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPreserveAspectRatio class

class nsSVGPreserveAspectRatio : public nsIDOMSVGPreserveAspectRatio,
                                 public nsSVGValue
{
protected:
  friend nsresult NS_NewSVGPreserveAspectRatio(
                                        nsIDOMSVGPreserveAspectRatio** result,
                                        PRUint16 aAlign,
                                        PRUint16 aMeetOrSlice);

  nsSVGPreserveAspectRatio(PRUint16 aAlign, PRUint16 aMeetOrSlice);
  ~nsSVGPreserveAspectRatio();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPreserveAspectRatio interface:
  NS_DECL_NSIDOMSVGPRESERVEASPECTRATIO

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

protected:
  PRUint16 mAlign, mMeetOrSlice;
};

//----------------------------------------------------------------------
// implementation:


nsSVGPreserveAspectRatio::nsSVGPreserveAspectRatio(PRUint16 aAlign,
                                                   PRUint16 aMeetOrSlice)
    : mAlign(aAlign), mMeetOrSlice(aMeetOrSlice)
{
}

nsSVGPreserveAspectRatio::~nsSVGPreserveAspectRatio()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGPreserveAspectRatio)
NS_IMPL_RELEASE(nsSVGPreserveAspectRatio)

NS_INTERFACE_MAP_BEGIN(nsSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGPreserveAspectRatio::SetValueString(const nsAString& aValue)
{
  char* str = ToNewCString(aValue);
  if (!str) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_OK;

  char* rest = str;
  char* token;
  const char* delimiters = "\x20\x9\xD\xA";
  PRUint16 align, meetOrSlice;

  token = nsCRT::strtok(rest, delimiters, &rest);

  if (token && !strcmp(token, "defer"))
    // Ignore: only applicable for preserveAspectRatio on 'image' elements
    token = nsCRT::strtok(rest, delimiters, &rest);

  if (token) {
    if (!strcmp(token, "none"))
      align = SVG_PRESERVEASPECTRATIO_NONE;
    else if (!strcmp(token, "xMinYMin"))
      align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
    else if (!strcmp(token, "xMidYMin"))
      align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
    else if (!strcmp(token, "xMaxYMin"))
      align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
    else if (!strcmp(token, "xMinYMid"))
      align = SVG_PRESERVEASPECTRATIO_XMINYMID;
    else if (!strcmp(token, "xMidYMid"))
      align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
    else if (!strcmp(token, "xMaxYMid"))
      align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
    else if (!strcmp(token, "xMinYMax"))
      align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
    else if (!strcmp(token, "xMidYMax"))
      align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
    else if (!strcmp(token, "xMaxYMax"))
      align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;
    else
      rv = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED(rv)) {
      token = nsCRT::strtok(rest, delimiters, &rest);
      if (token) {
        if (!strcmp(token, "meet"))
          meetOrSlice = SVG_MEETORSLICE_MEET;
        else if (!strcmp(token, "slice"))
          meetOrSlice = SVG_MEETORSLICE_SLICE;
        else
          rv = NS_ERROR_FAILURE;
      }
      else
        meetOrSlice = SVG_MEETORSLICE_MEET;
    }
  }
  else  // align not specified
    rv = NS_ERROR_FAILURE;

  if (nsCRT::strtok(rest, delimiters, &rest))  // there's more
    rv = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv)) {
    WillModify();
    mAlign = align;
    mMeetOrSlice = meetOrSlice;
    DidModify();
  }

  nsMemory::Free(str);

  return rv;
}

NS_IMETHODIMP
nsSVGPreserveAspectRatio::GetValueString(nsAString& aValue)
{
  // XXX: defer isn't stored

  switch (mAlign) {
    case SVG_PRESERVEASPECTRATIO_NONE:
      aValue.AssignLiteral("none");
      break;
    case SVG_PRESERVEASPECTRATIO_XMINYMIN:
      aValue.AssignLiteral("xMinYMin");
      break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      aValue.AssignLiteral("xMidYMin");
      break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
      aValue.AssignLiteral("xMaxYMin");
      break;
    case SVG_PRESERVEASPECTRATIO_XMINYMID:
      aValue.AssignLiteral("xMinYMid");
      break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMID:
      aValue.AssignLiteral("xMidYMid");
      break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMID:
      aValue.AssignLiteral("xMaxYMid");
      break;
    case SVG_PRESERVEASPECTRATIO_XMINYMAX:
      aValue.AssignLiteral("xMinYMax");
      break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
      aValue.AssignLiteral("xMidYMax");
      break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
      aValue.AssignLiteral("xMaxYMax");
      break;
    default:
      NS_NOTREACHED("Unknown value for mAlign");
  }

  // XXX: meetOrSlice may not have been specified in the attribute

  if (mAlign != SVG_PRESERVEASPECTRATIO_NONE) {
    switch (mMeetOrSlice) {
      case SVG_MEETORSLICE_MEET:
        aValue.AppendLiteral(" meet");
        break;
      case SVG_MEETORSLICE_SLICE:
        aValue.AppendLiteral(" slice");
        break;
      default:
        NS_NOTREACHED("Unknown value for mMeetOrSlice");
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPreserveAspectRatio methods:

/* attribute unsigned short align; */
NS_IMETHODIMP nsSVGPreserveAspectRatio::GetAlign(PRUint16 *aAlign)
{
  *aAlign = mAlign;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPreserveAspectRatio::SetAlign(PRUint16 aAlign)
{
  if (aAlign < SVG_PRESERVEASPECTRATIO_NONE ||
      aAlign > SVG_PRESERVEASPECTRATIO_XMAXYMAX)
    return NS_ERROR_FAILURE;

  WillModify();
  mAlign = aAlign;
  DidModify();

  return NS_OK;
}

/* attribute unsigned short meetOrSlice; */
NS_IMETHODIMP nsSVGPreserveAspectRatio::GetMeetOrSlice(PRUint16 *aMeetOrSlice)
{
  *aMeetOrSlice = mMeetOrSlice;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPreserveAspectRatio::SetMeetOrSlice(PRUint16 aMeetOrSlice)
{
  if (aMeetOrSlice < SVG_MEETORSLICE_MEET ||
      aMeetOrSlice > SVG_MEETORSLICE_SLICE)
    return NS_ERROR_FAILURE;

  WillModify();
  mMeetOrSlice = aMeetOrSlice;
  DidModify();

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGPreserveAspectRatio(nsIDOMSVGPreserveAspectRatio** result,
                             PRUint16 aAlign, PRUint16 aMeetOrSlice)
{
  *result = (nsIDOMSVGPreserveAspectRatio*) new nsSVGPreserveAspectRatio(aAlign, aMeetOrSlice);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*result);
  return NS_OK;
}
