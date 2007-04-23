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

#include "nsSVGRect.h"
#include "prdtoa.h"
#include "nsSVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsTextFormatter.h"
#include "nsCRT.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGLength.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"

////////////////////////////////////////////////////////////////////////
// nsSVGRect class

class nsSVGRect : public nsIDOMSVGRect,
                  public nsSVGValue
{
public:
  nsSVGRect(float x=0.0f, float y=0.0f, float w=0.0f, float h=0.0f);
  
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGRect interface:
  NS_DECL_NSIDOMSVGRECT

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);


protected:
  float mX, mY, mWidth, mHeight;
};

//----------------------------------------------------------------------
// implementation:

nsSVGRect::nsSVGRect(float x, float y, float w, float h)
    : mX(x), mY(y), mWidth(w), mHeight(h)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGRect)
NS_IMPL_RELEASE(nsSVGRect)

NS_INTERFACE_MAP_BEGIN(nsSVGRect)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGRect)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGRect)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGRect::SetValueString(const nsAString& aValue)
{
  nsresult rv = NS_OK;

  char* str = ToNewCString(aValue);

  char* rest = str;
  char* token;
  const char* delimiters = ",\x20\x9\xD\xA";

  double vals[4];
  int i;
  for (i=0;i<4;++i) {
    if (!(token = nsCRT::strtok(rest, delimiters, &rest))) break; // parse error

    char *end;
    vals[i] = PR_strtod(token, &end);
    if (*end != '\0') break; // parse error
  }
  if (i!=4 || (nsCRT::strtok(rest, delimiters, &rest)!=0)) {
    // there was a parse error.
    rv = NS_ERROR_FAILURE;
  }
  else {
    WillModify();
    mX      = float(vals[0]);
    mY      = float(vals[1]);
    mWidth  = float(vals[2]);
    mHeight = float(vals[3]);
    DidModify();
  }

  nsMemory::Free(str);

  return rv;
}

NS_IMETHODIMP
nsSVGRect::GetValueString(nsAString& aValue)
{
  PRUnichar buf[200];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g %g %g %g").get(),
                            (double)mX, (double)mY,
                            (double)mWidth, (double)mHeight);
  aValue.Assign(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGRect methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGRect::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGRect::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGRect::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGRect::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float width; */
NS_IMETHODIMP nsSVGRect::GetWidth(float *aWidth)
{
  *aWidth = mWidth;
  return NS_OK;
}
NS_IMETHODIMP nsSVGRect::SetWidth(float aWidth)
{
  WillModify();
  mWidth = aWidth;
  DidModify();
  return NS_OK;
}

/* attribute float height; */
NS_IMETHODIMP nsSVGRect::GetHeight(float *aHeight)
{
  *aHeight = mHeight;
  return NS_OK;
}
NS_IMETHODIMP nsSVGRect::SetHeight(float aHeight)
{
  WillModify();
  mHeight = aHeight;
  DidModify();
  return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// Implement a readonly version of SVGRect
//
// We need this because attributes of some SVG interfaces *and* the objects the
// attributes refer to (including SVGRects) are supposed to be readonly

class nsSVGReadonlyRect : public nsSVGRect
{
public:
  nsSVGReadonlyRect(float x, float y, float width, float height)
    : nsSVGRect(x, y, width, height)
  {
  }

  // override setters to make the whole object readonly
  NS_IMETHODIMP SetX(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetY(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetWidth(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetHeight(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetValueString(const nsAString&) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
};

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result, float x, float y,
              float width, float height)
{
  *result = new nsSVGRect(x, y, width, height);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
NS_NewSVGRect(nsIDOMSVGRect** result, const gfxRect& rect)
{
  return NS_NewSVGRect(result,
                       rect.X(), rect.Y(),
                       rect.Width(), rect.Height());
}

nsresult
NS_NewSVGReadonlyRect(nsIDOMSVGRect** result, float x, float y,
                      float width, float height)
{
  *result = new nsSVGReadonlyRect(x, y, width, height);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

