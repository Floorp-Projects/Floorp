/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com>
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
#if !defined(nsHTMLCanvasElement_h__)
#define nsHTMLCanvasElement_h__

#include "nsIDOMHTMLCanvasElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"

#include "nsIRenderingContext.h"

#include "nsICanvasRenderingContextInternal.h"
#include "nsICanvasElementExternal.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsLayoutUtils.h"

#include "Layers.h"

class nsIDOMFile;

class nsHTMLCanvasElement : public nsGenericHTMLElement,
                            public nsICanvasElementExternal,
                            public nsIDOMHTMLCanvasElement
{
  typedef mozilla::layers::CanvasLayer CanvasLayer;
  typedef mozilla::layers::LayerManager LayerManager;

public:
  nsHTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLCanvasElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLCanvasElement
  NS_DECL_NSIDOMHTMLCANVASELEMENT

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLCanvasElement,
                                                     nsGenericHTMLElement)

  /**
   * Ask the canvas Element to return the primary frame, if any
   */
  nsIFrame *GetPrimaryCanvasFrame();

  /**
   * Get the size in pixels of this canvas element
   */
  nsIntSize GetSize();

  /**
   * Determine whether the canvas is write-only.
   */
  PRBool IsWriteOnly();

  /**
   * Force the canvas to be write-only.
   */
  void SetWriteOnly();

  /*
   * Ask the canvas frame to invalidate itself.  If damageRect is
   * given, it is relative to the origin of the canvas frame in CSS pixels.
   */
  void InvalidateFrame(const gfxRect* damageRect = nsnull);

  /*
   * Get the number of contexts in this canvas, and request a context at
   * an index.
   */
  PRInt32 CountContexts ();
  nsICanvasRenderingContextInternal *GetContextAtIndex (PRInt32 index);

  /*
   * Returns true if the canvas context content is guaranteed to be opaque
   * across its entire area.
   */
  PRBool GetIsOpaque();

  /*
   * nsICanvasElementExternal -- for use outside of content/layout
   */
  NS_IMETHOD_(nsIntSize) GetSizeExternal();
  NS_IMETHOD RenderContextsExternal(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter);

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, PRInt32 aModType) const;

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  /*
   * Helpers called by various users of Canvas
   */

  already_AddRefed<CanvasLayer> GetCanvasLayer(CanvasLayer *aOldLayer,
                                               LayerManager *aManager);

  // Tell the Context that all the current rendering that it's
  // invalidated has been displayed to the screen, so that it should
  // start requesting invalidates again as needed.
  void MarkContextClean();

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  nsIntSize GetWidthHeight();

  nsresult UpdateContext();
  nsresult ExtractData(const nsAString& aType,
                       const nsAString& aOptions,
                       char*& aData,
                       PRUint32& aSize,
                       bool& aFellBackToPNG);
  nsresult ToDataURLImpl(const nsAString& aMimeType,
                         const nsAString& aEncoderOptions,
                         nsAString& aDataURL);
  nsresult MozGetAsFileImpl(const nsAString& aName,
                            const nsAString& aType,
                            nsIDOMFile** aResult);
  nsresult GetContextHelper(const nsAString& aContextId,
                            nsICanvasRenderingContextInternal **aContext);

  nsString mCurrentContextId;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
  
public:
  // Record whether this canvas should be write-only or not.
  // We set this when script paints an image from a different origin.
  // We also transitively set it when script paints a canvas which
  // is itself write-only.
  PRPackedBool             mWriteOnly;
};

#endif /* nsHTMLCanvasElement_h__ */
