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

#include "nsCOMPtr.h"
#include "nsIElementFactory.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsSVGAtoms.h"
#include "nsIXMLContent.h"


nsresult
NS_NewSVGPolylineElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGPolygonElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGCircleElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGEllipseElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGLineElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGRectElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGGElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGSVGElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGForeignObjectElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGPathElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGTextElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGTSpanElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGImageElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGStyleElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);
nsresult
NS_NewSVGDefsElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);


class nsSVGElementFactory : public nsIElementFactory
{
protected:
  nsSVGElementFactory();
  virtual ~nsSVGElementFactory();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIElementFactory interface
  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo, nsIContent** aResult);
  
public:
  friend nsresult NS_NewSVGElementFactory(nsIElementFactory** aResult);
};



nsSVGElementFactory::nsSVGElementFactory()
{
}

nsSVGElementFactory::~nsSVGElementFactory()
{
  
}


NS_IMPL_ISUPPORTS1(nsSVGElementFactory, nsIElementFactory)


nsresult
NS_NewSVGElementFactory(nsIElementFactory** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsSVGElementFactory* result = new nsSVGElementFactory();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}



NS_IMETHODIMP
nsSVGElementFactory::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                           nsIContent** aResult)
{
  nsIAtom *name = aNodeInfo->NameAtom();
  
  if (name == nsSVGAtoms::polyline)
    return NS_NewSVGPolylineElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::polygon)
    return NS_NewSVGPolygonElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::circle)
    return NS_NewSVGCircleElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::ellipse)
    return NS_NewSVGEllipseElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::line)
    return NS_NewSVGLineElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::rect)
    return NS_NewSVGRectElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::svg)
    return NS_NewSVGSVGElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::g)
    return NS_NewSVGGElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::foreignObject)
    return NS_NewSVGForeignObjectElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::path)
    return NS_NewSVGPathElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::text)
    return NS_NewSVGTextElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::tspan)
    return NS_NewSVGTSpanElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::image)
    return NS_NewSVGImageElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::style)
    return NS_NewSVGStyleElement(aResult, aNodeInfo);
  if (name == nsSVGAtoms::defs)
    return NS_NewSVGDefsElement(aResult, aNodeInfo);

  // if we don't know what to create, just create a standard xml element:
  return NS_NewXMLElement(aResult, aNodeInfo);
}

