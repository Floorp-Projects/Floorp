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

#include "nsSVGPathSeg.h"
#include "prdtoa.h"
#include "nsSVGValue.h"
#include "nsTextFormatter.h"

//----------------------------------------------------------------------
// implementation helper macros

#define NS_IMPL_NSIDOMSVGPATHSEG(cname, type, letter)                   \
NS_IMETHODIMP                                                           \
cname::GetPathSegType(PRUint16 *aPathSegType)                           \
{                                                                       \
  *aPathSegType = type;                                                 \
  return NS_OK;                                                         \
}                                                                       \
                                                                        \
NS_IMETHODIMP                                                           \
cname::GetPathSegTypeAsLetter(nsAString & aPathSegTypeAsLetter)         \
{                                                                       \
  aPathSegTypeAsLetter.Truncate();                                      \
  aPathSegTypeAsLetter.AppendLiteral(letter);               \
  return NS_OK;                                                         \
}

#define NS_IMPL_NSISUPPORTS_SVGPATHSEG(basename)              \
NS_IMPL_ADDREF(ns##basename)                                  \
NS_IMPL_RELEASE(ns##basename)                                 \
                                                              \
NS_INTERFACE_MAP_BEGIN(ns##basename)                          \
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)                         \
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathSeg)                    \
  NS_INTERFACE_MAP_ENTRY(nsIDOM##basename)                    \
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(basename)          \
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)  \
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegClosePath

class nsSVGPathSegClosePath : public nsIDOMSVGPathSegClosePath,
                              public nsSVGValue
{
public:
  nsSVGPathSegClosePath();

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG

  // nsIDOMSVGPathSegClosePath interface:
  NS_DECL_NSIDOMSVGPATHSEGCLOSEPATH
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegClosePath(nsIDOMSVGPathSegClosePath** result)
{
  nsSVGPathSegClosePath *ps = new nsSVGPathSegClosePath();
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegClosePath::nsSVGPathSegClosePath()
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegClosePath)
  
//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegClosePath::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegClosePath::GetValueString(nsAString& aValue)
{
  aValue.Truncate();
  aValue.AppendLiteral("z");
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegClosePath, PATHSEG_CLOSEPATH, "z")

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegMovetoAbs

class nsSVGPathSegMovetoAbs : public nsIDOMSVGPathSegMovetoAbs,
                              public nsSVGValue
{
public:
  nsSVGPathSegMovetoAbs(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegMovetoAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGMOVETOABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegMovetoAbs(nsIDOMSVGPathSegMovetoAbs** result,
                          float x, float y)
{ 
  nsSVGPathSegMovetoAbs *ps = new nsSVGPathSegMovetoAbs(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegMovetoAbs::nsSVGPathSegMovetoAbs(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegMovetoAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegMovetoAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegMovetoAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[48];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("M%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegMovetoAbs, PATHSEG_MOVETO_ABS, "M")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegMovetoAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegMovetoAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegMovetoAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegMovetoAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegMovetoAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsSVGPathSegMovetoRel

class nsSVGPathSegMovetoRel : public nsIDOMSVGPathSegMovetoRel,
                              public nsSVGValue
{
public:
  nsSVGPathSegMovetoRel(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegMovetoRel interface:
  NS_DECL_NSIDOMSVGPATHSEGMOVETOREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegMovetoRel(nsIDOMSVGPathSegMovetoRel** result,
                          float x, float y)
{
  nsSVGPathSegMovetoRel *ps = new nsSVGPathSegMovetoRel(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegMovetoRel::nsSVGPathSegMovetoRel(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegMovetoRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegMovetoRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegMovetoRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[48];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("m%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegMovetoRel, PATHSEG_MOVETO_REL, "m")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegMovetoRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegMovetoRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegMovetoRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegMovetoRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegMovetoRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}  

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoAbs

class nsSVGPathSegLinetoAbs : public nsIDOMSVGPathSegLinetoAbs,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoAbs(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoAbs(nsIDOMSVGPathSegLinetoAbs** result,
                          float x, float y)
{
  nsSVGPathSegLinetoAbs *ps = new nsSVGPathSegLinetoAbs(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoAbs::nsSVGPathSegLinetoAbs(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[48];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("L%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoAbs, PATHSEG_LINETO_ABS, "L")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegLinetoAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegLinetoAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

  
////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoRel

class nsSVGPathSegLinetoRel : public nsIDOMSVGPathSegLinetoRel,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoRel(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoRel interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoRel(nsIDOMSVGPathSegLinetoRel** result,
                          float x, float y)
{
  nsSVGPathSegLinetoRel *ps = new nsSVGPathSegLinetoRel(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoRel::nsSVGPathSegLinetoRel(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[48];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("l%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoRel, PATHSEG_LINETO_REL, "l")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegLinetoRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegLinetoRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

  
////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoCubicAbs

class nsSVGPathSegCurvetoCubicAbs : public nsIDOMSVGPathSegCurvetoCubicAbs,
                                    public nsSVGValue
{
public:
  nsSVGPathSegCurvetoCubicAbs(float x, float y,
                              float x1, float y1,
                              float x2, float y2);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoCubicAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX1, mY1, mX2, mY2;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoCubicAbs(nsIDOMSVGPathSegCurvetoCubicAbs** result,
                                float x, float y,
                                float x1, float y1,
                                float x2, float y2)
{
  nsSVGPathSegCurvetoCubicAbs *ps = new nsSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoCubicAbs::nsSVGPathSegCurvetoCubicAbs(float x, float y,
                                                         float x1, float y1,
                                                         float x2, float y2)
    : mX(x), mY(y), mX1(x1), mY1(y1), mX2(x2), mY2(y2)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoCubicAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoCubicAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoCubicAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[144];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("C%g,%g %g,%g %g,%g").get(), 
                            mX1, mY1, mX2, mY2, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoCubicAbs, PATHSEG_CURVETO_CUBIC_ABS, "C")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoCubicAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x1; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetX1(float *aX1)
{
  *aX1 = mX1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetX1(float aX1)
{
  WillModify();
  mX1 = aX1;
  DidModify();
  return NS_OK;
}

/* attribute float y1; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetY1(float *aY1)
{
  *aY1 = mY1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetY1(float aY1)
{
  WillModify();
  mY1 = aY1;
  DidModify();
  return NS_OK;
}

/* attribute float x2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetX2(float *aX2)
{
  *aX2 = mX2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetX2(float aX2)
{
  WillModify();
  mX2 = aX2;
  DidModify();
  return NS_OK;
}

/* attribute float y2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::GetY2(float *aY2)
{
  *aY2 = mY2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicAbs::SetY2(float aY2)
{
  WillModify();
  mY2 = aY2;
  DidModify();
  return NS_OK;
}

  
////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoCubicRel

class nsSVGPathSegCurvetoCubicRel : public nsIDOMSVGPathSegCurvetoCubicRel,
                                    public nsSVGValue
{
public:
  nsSVGPathSegCurvetoCubicRel(float x, float y,
                              float x1, float y1,
                              float x2, float y2);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoCubicRel interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX1, mY1, mX2, mY2;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoCubicRel(nsIDOMSVGPathSegCurvetoCubicRel** result,
                                float x, float y,
                                float x1, float y1,
                                float x2, float y2)
{
  nsSVGPathSegCurvetoCubicRel *ps = new nsSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoCubicRel::nsSVGPathSegCurvetoCubicRel(float x, float y,
                                                         float x1, float y1,
                                                         float x2, float y2)
    : mX(x), mY(y), mX1(x1), mY1(y1), mX2(x2), mY2(y2)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoCubicRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoCubicRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoCubicRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[144];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("c%g,%g %g,%g %g,%g").get(),
                            mX1, mY1, mX2, mY2, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoCubicRel, PATHSEG_CURVETO_CUBIC_REL, "c")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoCubicRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x1; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetX1(float *aX1)
{
  *aX1 = mX1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetX1(float aX1)
{
  WillModify();
  mX1 = aX1;
  DidModify();
  return NS_OK;
}

/* attribute float y1; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetY1(float *aY1)
{
  *aY1 = mY1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetY1(float aY1)
{
  WillModify();
  mY1 = aY1;
  DidModify();
  return NS_OK;
}

/* attribute float x2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetX2(float *aX2)
{
  *aX2 = mX2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetX2(float aX2)
{
  WillModify();
  mX2 = aX2;
  DidModify();
  return NS_OK;
}

/* attribute float y2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::GetY2(float *aY2)
{
  *aY2 = mY2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicRel::SetY2(float aY2)
{
  WillModify();
  mY2 = aY2;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoQuadraticAbs

class nsSVGPathSegCurvetoQuadraticAbs : public nsIDOMSVGPathSegCurvetoQuadraticAbs,
                                        public nsSVGValue
{
public:
  nsSVGPathSegCurvetoQuadraticAbs(float x, float y,
                                  float x1, float y1);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoQuadraticAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX1, mY1;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoQuadraticAbs(nsIDOMSVGPathSegCurvetoQuadraticAbs** result,
                                    float x, float y,
                                    float x1, float y1)
{
  nsSVGPathSegCurvetoQuadraticAbs *ps = new nsSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoQuadraticAbs::nsSVGPathSegCurvetoQuadraticAbs(float x, float y,
                                                                 float x1, float y1)
    : mX(x), mY(y), mX1(x1), mY1(y1)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoQuadraticAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[96];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("Q%g,%g %g,%g").get(),
                            mX1, mY1, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoQuadraticAbs, PATHSEG_CURVETO_QUADRATIC_ABS, "Q")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoQuadraticAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x1; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::GetX1(float *aX1)
{
  *aX1 = mX1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::SetX1(float aX1)
{
  WillModify();
  mX1 = aX1;
  DidModify();
  return NS_OK;
}

/* attribute float y1; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::GetY1(float *aY1)
{
  *aY1 = mY1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticAbs::SetY1(float aY1)
{
  WillModify();
  mY1 = aY1;
  DidModify();
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoQuadraticRel

class nsSVGPathSegCurvetoQuadraticRel : public nsIDOMSVGPathSegCurvetoQuadraticRel,
                                        public nsSVGValue
{
public:
  nsSVGPathSegCurvetoQuadraticRel(float x, float y,
                                  float x1, float y1);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoQuadraticRel interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX1, mY1;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoQuadraticRel(nsIDOMSVGPathSegCurvetoQuadraticRel** result,
                                    float x, float y,
                                    float x1, float y1)
{
  nsSVGPathSegCurvetoQuadraticRel *ps = new nsSVGPathSegCurvetoQuadraticRel(x, y, x1, y1);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoQuadraticRel::nsSVGPathSegCurvetoQuadraticRel(float x, float y,
                                                                 float x1, float y1)
    : mX(x), mY(y), mX1(x1), mY1(y1)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoQuadraticRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[96];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("q%g,%g %g,%g").get(), 
                            mX1, mY1, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoQuadraticRel, PATHSEG_CURVETO_QUADRATIC_REL, "q")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoQuadraticRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x1; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::GetX1(float *aX1)
{
  *aX1 = mX1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::SetX1(float aX1)
{
  WillModify();
  mX1 = aX1;
  DidModify();
  return NS_OK;
}

/* attribute float y1; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::GetY1(float *aY1)
{
  *aY1 = mY1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticRel::SetY1(float aY1)
{
  WillModify();
  mY1 = aY1;
  DidModify();
  return NS_OK;
}

  
////////////////////////////////////////////////////////////////////////
// nsSVGPathSegArcAbs

class nsSVGPathSegArcAbs : public nsIDOMSVGPathSegArcAbs,
                                        public nsSVGValue
{
public:
  nsSVGPathSegArcAbs(float x, float y,
                     float r1, float r2, float angle,
                     PRBool largeArcFlag, PRBool sweepFlag);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegArcAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGARCABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float  mX, mY, mR1, mR2, mAngle;
  PRBool mLargeArcFlag, mSweepFlag;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegArcAbs(nsIDOMSVGPathSegArcAbs** result,
                       float x, float y,
                       float r1, float r2, float angle,
                       PRBool largeArcFlag, PRBool sweepFlag)
{
  nsSVGPathSegArcAbs *ps = new nsSVGPathSegArcAbs(x, y, r1, r2, angle,
                                                  largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegArcAbs::nsSVGPathSegArcAbs(float x, float y,
                                       float r1, float r2, float angle,
                                       PRBool largeArcFlag, PRBool sweepFlag)
    : mX(x), mY(y), mR1(r1), mR2(r2), mAngle(angle),
      mLargeArcFlag(largeArcFlag), mSweepFlag(sweepFlag)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegArcAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegArcAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegArcAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[168];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("A%g,%g %g %d,%d %g,%g").get(), 
                            mR1, mR2, mAngle, mLargeArcFlag, mSweepFlag, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegArcAbs, PATHSEG_ARC_ABS, "A")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegArcAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float r1; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetR1(float *aR1)
{
  *aR1 = mR1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetR1(float aR1)
{
  WillModify();
  mR1 = aR1;
  DidModify();
  return NS_OK;
}

/* attribute float r2; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetR2(float *aR2)
{
  *aR2 = mR2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetR2(float aR2)
{
  WillModify();
  mR2 = aR2;
  DidModify();
  return NS_OK;
}


/* attribute float angle; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetAngle(float *aAngle)
{
  *aAngle = mAngle;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetAngle(float aAngle)
{
  WillModify();
  mAngle = aAngle;
  DidModify();
  return NS_OK;
}

/* attribute boolean largeArcFlag; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetLargeArcFlag(PRBool *aLargeArcFlag)
{
  *aLargeArcFlag = mLargeArcFlag;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetLargeArcFlag(PRBool aLargeArcFlag)
{
  WillModify();
  mLargeArcFlag = aLargeArcFlag;
  DidModify();
  return NS_OK;
}

/* attribute boolean sweepFlag; */
NS_IMETHODIMP nsSVGPathSegArcAbs::GetSweepFlag(PRBool *aSweepFlag)
{
  *aSweepFlag = mSweepFlag;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcAbs::SetSweepFlag(PRBool aSweepFlag)
{
  WillModify();
  mSweepFlag = aSweepFlag;
  DidModify();
  return NS_OK;  
}


////////////////////////////////////////////////////////////////////////
// nsSVGPathSegArcRel

class nsSVGPathSegArcRel : public nsIDOMSVGPathSegArcRel,
                                        public nsSVGValue
{
public:
  nsSVGPathSegArcRel(float x, float y,
                     float r1, float r2, float angle,
                     PRBool largeArcFlag, PRBool sweepFlag);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegArcRel interface:
  NS_DECL_NSIDOMSVGPATHSEGARCREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float  mX, mY, mR1, mR2, mAngle;
  PRBool mLargeArcFlag, mSweepFlag;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegArcRel(nsIDOMSVGPathSegArcRel** result,
                       float x, float y,
                       float r1, float r2, float angle,
                       PRBool largeArcFlag, PRBool sweepFlag)
{
  nsSVGPathSegArcRel *ps = new nsSVGPathSegArcRel(x, y, r1, r2, angle,
                                                  largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegArcRel::nsSVGPathSegArcRel(float x, float y,
                                       float r1, float r2, float angle,
                                       PRBool largeArcFlag, PRBool sweepFlag)
    : mX(x), mY(y), mR1(r1), mR2(r2), mAngle(angle),
      mLargeArcFlag(largeArcFlag), mSweepFlag(sweepFlag)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegArcRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegArcRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegArcRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[168];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("a%g,%g %g %d,%d %g,%g").get(), 
                            mR1, mR2, mAngle, mLargeArcFlag, mSweepFlag, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegArcRel, PATHSEG_ARC_REL, "a")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegArcRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float r1; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetR1(float *aR1)
{
  *aR1 = mR1;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetR1(float aR1)
{
  WillModify();
  mR1 = aR1;
  DidModify();
  return NS_OK;
}

/* attribute float r2; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetR2(float *aR2)
{
  *aR2 = mR2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetR2(float aR2)
{
  WillModify();
  mR2 = aR2;
  DidModify();
  return NS_OK;
}


/* attribute float angle; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetAngle(float *aAngle)
{
  *aAngle = mAngle;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetAngle(float aAngle)
{
  WillModify();
  mAngle = aAngle;
  DidModify();
  return NS_OK;
}

/* attribute boolean largeArcFlag; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetLargeArcFlag(PRBool *aLargeArcFlag)
{
  *aLargeArcFlag = mLargeArcFlag;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetLargeArcFlag(PRBool aLargeArcFlag)
{
  WillModify();
  mLargeArcFlag = aLargeArcFlag;
  DidModify();
  return NS_OK;
}

/* attribute boolean sweepFlag; */
NS_IMETHODIMP nsSVGPathSegArcRel::GetSweepFlag(PRBool *aSweepFlag)
{
  *aSweepFlag = mSweepFlag;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegArcRel::SetSweepFlag(PRBool aSweepFlag)
{
  WillModify();
  mSweepFlag = aSweepFlag;
  DidModify();
  return NS_OK;  
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoHorizontalAbs

class nsSVGPathSegLinetoHorizontalAbs : public nsIDOMSVGPathSegLinetoHorizontalAbs,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoHorizontalAbs(float x);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoHorizontalAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOHORIZONTALABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoHorizontalAbs(nsIDOMSVGPathSegLinetoHorizontalAbs** result, float x)
{
  nsSVGPathSegLinetoHorizontalAbs *ps = new nsSVGPathSegLinetoHorizontalAbs(x);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoHorizontalAbs::nsSVGPathSegLinetoHorizontalAbs(float x)
    : mX(x)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoHorizontalAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoHorizontalAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoHorizontalAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("H%g").get(), mX);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoHorizontalAbs, PATHSEG_LINETO_HORIZONTAL_ABS, "H")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoHorizontalAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegLinetoHorizontalAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoHorizontalAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoHorizontalRel

class nsSVGPathSegLinetoHorizontalRel : public nsIDOMSVGPathSegLinetoHorizontalRel,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoHorizontalRel(float x);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoHorizontalRel interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOHORIZONTALREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoHorizontalRel(nsIDOMSVGPathSegLinetoHorizontalRel** result, float x)
{
  nsSVGPathSegLinetoHorizontalRel *ps = new nsSVGPathSegLinetoHorizontalRel(x);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoHorizontalRel::nsSVGPathSegLinetoHorizontalRel(float x)
    : mX(x)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoHorizontalRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoHorizontalRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoHorizontalRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("h%g").get(), mX);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoHorizontalRel, PATHSEG_LINETO_HORIZONTAL_REL, "h")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoHorizontalRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegLinetoHorizontalRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoHorizontalRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoVerticalAbs

class nsSVGPathSegLinetoVerticalAbs : public nsIDOMSVGPathSegLinetoVerticalAbs,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoVerticalAbs(float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoVerticalAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOVERTICALABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoVerticalAbs(nsIDOMSVGPathSegLinetoVerticalAbs** result, float y)
{
  nsSVGPathSegLinetoVerticalAbs *ps = new nsSVGPathSegLinetoVerticalAbs(y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoVerticalAbs::nsSVGPathSegLinetoVerticalAbs(float y)
    : mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoVerticalAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoVerticalAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoVerticalAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("V%g").get(), mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoVerticalAbs, PATHSEG_LINETO_VERTICAL_ABS, "V")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoVerticalAbs methods:

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegLinetoVerticalAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoVerticalAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegLinetoVerticalRel

class nsSVGPathSegLinetoVerticalRel : public nsIDOMSVGPathSegLinetoVerticalRel,
                              public nsSVGValue
{
public:
  nsSVGPathSegLinetoVerticalRel(float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegLinetoVerticalRel interface:
  NS_DECL_NSIDOMSVGPATHSEGLINETOVERTICALREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegLinetoVerticalRel(nsIDOMSVGPathSegLinetoVerticalRel** result, float y)
{
  nsSVGPathSegLinetoVerticalRel *ps = new nsSVGPathSegLinetoVerticalRel(y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegLinetoVerticalRel::nsSVGPathSegLinetoVerticalRel(float y)
    : mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegLinetoVerticalRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegLinetoVerticalRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegLinetoVerticalRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("v%g").get(), mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegLinetoVerticalRel, PATHSEG_LINETO_VERTICAL_REL, "v")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegLinetoVerticalRel methods:

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegLinetoVerticalRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegLinetoVerticalRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoCubicSmoothAbs

class nsSVGPathSegCurvetoCubicSmoothAbs : public nsIDOMSVGPathSegCurvetoCubicSmoothAbs,
                                          public nsSVGValue
{
public:
  nsSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                    float x2, float y2);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoCubicSmoothAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICSMOOTHABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX2, mY2;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoCubicSmoothAbs(nsIDOMSVGPathSegCurvetoCubicSmoothAbs** result,
                                      float x, float y,
                                      float x2, float y2)
{
  nsSVGPathSegCurvetoCubicSmoothAbs *ps = new nsSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoCubicSmoothAbs::nsSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                                                     float x2, float y2)
    : mX(x), mY(y), mX2(x2), mY2(y2)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoCubicSmoothAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoCubicSmoothAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoCubicSmoothAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[96];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("S%g,%g %g,%g").get(),
                            mX2, mY2, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoCubicSmoothAbs, PATHSEG_CURVETO_CUBIC_SMOOTH_ABS, "S")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoCubicSmoothAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::GetX2(float *aX2)
{
  *aX2 = mX2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::SetX2(float aX2)
{
  WillModify();
  mX2 = aX2;
  DidModify();
  return NS_OK;
}

/* attribute float y2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::GetY2(float *aY2)
{
  *aY2 = mY2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothAbs::SetY2(float aY2)
{
  WillModify();
  mY2 = aY2;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoCubicSmoothRel

class nsSVGPathSegCurvetoCubicSmoothRel : public nsIDOMSVGPathSegCurvetoCubicSmoothRel,
                                          public nsSVGValue
{
public:
  nsSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                    float x2, float y2);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoCubicSmoothRel interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICSMOOTHREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY, mX2, mY2;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoCubicSmoothRel(nsIDOMSVGPathSegCurvetoCubicSmoothRel** result,
                                      float x, float y,
                                      float x2, float y2)
{
  nsSVGPathSegCurvetoCubicSmoothRel *ps = new nsSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoCubicSmoothRel::nsSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                                                     float x2, float y2)
    : mX(x), mY(y), mX2(x2), mY2(y2)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoCubicSmoothRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoCubicSmoothRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoCubicSmoothRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[96];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("s%g,%g %g,%g").get(),
                            mX2, mY2, mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoCubicSmoothRel, PATHSEG_CURVETO_CUBIC_SMOOTH_REL, "s")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoCubicSmoothRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

/* attribute float x2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::GetX2(float *aX2)
{
  *aX2 = mX2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::SetX2(float aX2)
{
  WillModify();
  mX2 = aX2;
  DidModify();
  return NS_OK;
}

/* attribute float y2; */
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::GetY2(float *aY2)
{
  *aY2 = mY2;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoCubicSmoothRel::SetY2(float aY2)
{
  WillModify();
  mY2 = aY2;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoQuadraticSmoothAbs

class nsSVGPathSegCurvetoQuadraticSmoothAbs : public nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs,
                                              public nsSVGValue
{
public:
  nsSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICSMOOTHABS

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs** result,
                          float x, float y)
{
  nsSVGPathSegCurvetoQuadraticSmoothAbs *ps = new nsSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoQuadraticSmoothAbs::nsSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoQuadraticSmoothAbs)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticSmoothAbs::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticSmoothAbs::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[48];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("T%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoQuadraticSmoothAbs, PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS, "T")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothAbs::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothAbs::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothAbs::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothAbs::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegCurvetoQuadraticSmoothRel

class nsSVGPathSegCurvetoQuadraticSmoothRel : public nsIDOMSVGPathSegCurvetoQuadraticSmoothRel,
                                              public nsSVGValue
{
public:
  nsSVGPathSegCurvetoQuadraticSmoothRel(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegCurvetoQuadraticSmoothRel interface:
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICSMOOTHREL

  // nsIDOMSVGPathSeg interface:
  NS_DECL_NSIDOMSVGPATHSEG
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  float mX, mY;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPathSegCurvetoQuadraticSmoothRel(nsIDOMSVGPathSegCurvetoQuadraticSmoothRel** result,
                                          float x, float y)
{
  nsSVGPathSegCurvetoQuadraticSmoothRel *ps = new nsSVGPathSegCurvetoQuadraticSmoothRel(x, y);
  NS_ENSURE_TRUE(ps, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(ps);
  *result = ps;
  return NS_OK;
}


nsSVGPathSegCurvetoQuadraticSmoothRel::nsSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:
NS_IMPL_NSISUPPORTS_SVGPATHSEG(SVGPathSegCurvetoQuadraticSmoothRel)

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticSmoothRel::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGPathSegCurvetoQuadraticSmoothRel::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("t%g,%g").get(), mX, mY);
  aValue.Append(buf);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSeg methods:
NS_IMPL_NSIDOMSVGPATHSEG(nsSVGPathSegCurvetoQuadraticSmoothRel, PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL, "t")

//----------------------------------------------------------------------
// nsIDOMSVGPathSegCurvetoQuadraticSmoothRel methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothRel::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothRel::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothRel::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPathSegCurvetoQuadraticSmoothRel::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  return NS_OK;
}

