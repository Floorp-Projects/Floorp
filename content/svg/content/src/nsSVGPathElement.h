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

#ifndef __NS_SVGPATHELEMENT_H__
#define __NS_SVGPATHELEMENT_H__

#include "nsSVGPathGeometryElement.h"
#include "nsIDOMSVGPathElement.h"
#include "nsIDOMSVGAnimatedPathData.h"
#include "nsSVGNumber2.h"
#include "gfxPath.h"

class gfxContext;

class nsSVGPathList
{
friend class nsSVGPathDataParserToInternal;

public:
  enum { MOVETO, LINETO, CURVETO, CLOSEPATH };
  nsSVGPathList() : mArguments(nsnull), mNumCommands(0), mNumArguments(0) {}
  ~nsSVGPathList() { Clear(); }
  void Playback(gfxContext *aCtx);

protected:
  void Clear();

  float   *mArguments;
  PRUint32 mNumCommands;
  PRUint32 mNumArguments;
};

typedef nsSVGPathGeometryElement nsSVGPathElementBase;

class nsSVGPathElement : public nsSVGPathElementBase,
                         public nsIDOMSVGPathElement,
                         public nsIDOMSVGAnimatedPathData
{
friend class nsSVGPathFrame;

protected:
  friend nsresult NS_NewSVGPathElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGPathElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGPathElement();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGPATHELEMENT
  NS_DECL_NSIDOMSVGANIMATEDPATHDATA

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGPathElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGPathElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGPathElementBase::)

  // nsIContent interface
  NS_IMETHODIMP_(PRBool) IsAttributeMapped(const nsIAtom* name) const;

  // nsISVGValueObserver
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsSVGPathGeometryElement methods:
  virtual PRBool IsDependentAttribute(nsIAtom *aName);
  virtual PRBool IsMarkable();
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(gfxContext *aCtx);

  virtual already_AddRefed<gfxFlattenedPath> GetFlattenedPath(nsIDOMSVGMatrix *aMatrix);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual NumberAttributesInfo GetNumberInfo();
  virtual nsresult BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAString* aValue, PRBool aNotify);

  // Helper for lazily creating pathseg list
  nsresult CreatePathSegList();

  nsCOMPtr<nsIDOMSVGPathSegList> mSegments;
  nsSVGNumber2 mPathLength;
  static NumberInfo sNumberInfo;
  nsSVGPathList mPathData;
};

#endif
