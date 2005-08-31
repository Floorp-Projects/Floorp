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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsIDOMHTMLCanvasElement.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"

#include "nsICanvasElement.h"

#include "gfxIImageFrame.h"
#include "imgIContainer.h"

#include "nsICanvasRenderingContextInternal.h"

#define DEFAULT_CANVAS_WIDTH 300
#define DEFAULT_CANVAS_HEIGHT 150

class nsHTMLCanvasElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLCanvasElement,
                            public nsICanvasElement
{
public:
  nsHTMLCanvasElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLCanvasElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLCanvasElement
  NS_DECL_NSIDOMHTMLCANVASELEMENT

  // nsICanvasElement
  NS_IMETHOD GetCanvasImageContainer(imgIContainer **aImageContainer);
  NS_IMETHOD GetPrimaryCanvasFrame(nsIFrame **aFrame);
  NS_IMETHOD UpdateImageFrame();

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  PRBool ParseAttribute(nsIAtom* aAttribute, const nsAString& aValue, nsAttrValue& aResult);
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
protected:
  nsIntSize GetWidthHeight();
  nsresult UpdateImageContainer(PRBool forceCreate);
  nsresult UpdateContext();

  nsString mCurrentContextId;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;

  nsCOMPtr<imgIContainer> mImageContainer;
  nsCOMPtr<gfxIImageFrame> mImageFrame;
};

nsGenericHTMLElement*
NS_NewHTMLCanvasElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  return new nsHTMLCanvasElement(aNodeInfo);
}

nsHTMLCanvasElement::nsHTMLCanvasElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLCanvasElement::~nsHTMLCanvasElement()
{
  if (mCurrentContext) {
    nsCOMPtr<nsICanvasRenderingContextInternal> internalctx(do_QueryInterface(mCurrentContext));
    internalctx->SetCanvasElement(nsnull);
    mCurrentContext = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsHTMLCanvasElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLCanvasElement, nsGenericElement)

NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLCanvasElement, nsGenericElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCanvasElement)
  NS_INTERFACE_MAP_ENTRY(nsICanvasElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLCanvasElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_DOM_CLONENODE(nsHTMLCanvasElement)

nsIntSize
nsHTMLCanvasElement::GetWidthHeight()
{
  nsIntSize size(0,0);
  const nsAttrValue* value;

  if ((value = GetParsedAttr(nsHTMLAtoms::width)) &&
      value->Type() == nsAttrValue::eInteger)
  {
      size.width = value->GetIntegerValue();
  }

  if ((value = GetParsedAttr(nsHTMLAtoms::height)) &&
      value->Type() == nsAttrValue::eInteger)
  {
      size.height = value->GetIntegerValue();
  }

  if (size.width <= 0)
    size.width = DEFAULT_CANVAS_WIDTH;
  if (size.height <= 0)
    size.height = DEFAULT_CANVAS_HEIGHT;

  return size;
}

NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Width, width, DEFAULT_CANVAS_WIDTH)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Height, height, DEFAULT_CANVAS_HEIGHT)

nsresult
nsHTMLCanvasElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                              aNotify);
  if (NS_SUCCEEDED(rv) && mCurrentContext &&
      (aName == nsHTMLAtoms::width || aName == nsHTMLAtoms::height))
  {
    rv = UpdateImageContainer(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsChangeHint
nsHTMLCanvasElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsHTMLAtoms::width ||
      aAttribute == nsHTMLAtoms::height)
  {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsMapRuleToAttributesFunc
nsHTMLCanvasElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

static const nsGenericElement::MappedAttributeEntry
sImageMarginAttributeMap[] = {
  { &nsHTMLAtoms::hspace },
  { &nsHTMLAtoms::vspace },
  { nsnull }
};

NS_IMETHODIMP_(PRBool)
nsHTMLCanvasElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginAttributeMap
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

PRBool
nsHTMLCanvasElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height))
  {
    return aResult.ParseIntWithBounds(aValue, 0);
  }

  if (ParseImageAttribute(aAttribute, aValue, aResult))
    return PR_TRUE;

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}


// nsHTMLCanvasElement::toDataURL
//
//    This is the version in the spec, it returns a PNG with transparency

NS_IMETHODIMP
nsHTMLCanvasElement::ToDataURL(nsAString& aDataURL)
{
  return ToDataURLAs(NS_LITERAL_STRING("image/png"), EmptyString(),
                     aDataURL);
}


// nsHTMLCanvasElement::toDataURLAs
//
//    http://www.mozilla.org/quality/networking/docs/aboutdata.html
//
//    Get the data
//    encode it as PNG
//    base64 encode it
NS_IMETHODIMP
nsHTMLCanvasElement::ToDataURLAs(const nsAString& aMimeType,
                                 const nsAString& aEncoderOptions,
                                 nsAString& aDataURL)
{
  nsresult rv;

  // We get an input stream from the context. If more than one context type
  // is supported in the future, this will have to be changed to do the right
  // thing. For now, just assume that the 2D context has all the goods.
  nsCOMPtr<nsICanvasRenderingContextInternal> context;
  rv = GetContext(NS_LITERAL_STRING("2d"), getter_AddRefs(context));
  NS_ENSURE_SUCCESS(rv, rv);

  // get image bytes
  nsCOMPtr<nsIInputStream> imgStream;
  NS_ConvertUTF16toUTF8 aMimeType8(aMimeType);
  rv = context->GetInputStream(aMimeType8, aEncoderOptions,
                               getter_AddRefs(imgStream));
  NS_ENSURE_SUCCESS(rv, rv);

  // Generally, there will be only one chunk of data, and it will be available
  // for us to read right away, so optimize this case.
  PRUint32 bufSize;
  rv = imgStream->Available(&bufSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // ...leave a little extra room so we can call read again and make sure we
  // got everything. 16 bytes for better padding (maybe)
  bufSize += 16;
  PRUint32 imgSize = 0;
  char* imgData = (char*)PR_Malloc(bufSize);
  if (! imgData)
    return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 numReadThisTime = 0;
  while ((rv = imgStream->Read(&imgData[imgSize], bufSize - imgSize,
                         &numReadThisTime)) == NS_OK && numReadThisTime > 0) {
    imgSize += numReadThisTime;
    if (imgSize == bufSize) {
      // need a bigger buffer, just double
      bufSize *= 2;
      char* newImgData = (char*)PR_Realloc(imgData, bufSize);
      if (! newImgData) {
        PR_Free(imgData);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      imgData = newImgData;
    }
  }

  // base 64, result will be NULL terminated
  char* encodedImg = PL_Base64Encode(imgData, imgSize, nsnull);
  PR_Free(imgData);
  if (!encodedImg) // not sure why this would fail
    return NS_ERROR_OUT_OF_MEMORY;

  // build data URL string
  aDataURL = NS_LITERAL_STRING("data:") + aMimeType +
    NS_LITERAL_STRING(";base64,") + NS_ConvertUTF8toUTF16(encodedImg);

  PR_Free(encodedImg);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetContext(const nsAString& aContextId,
                                nsISupports **aContext)
{
  nsresult rv;

  if (mCurrentContextId.IsEmpty()) {
    nsCString ctxId;
    ctxId.Assign(NS_LossyConvertUTF16toASCII(aContextId));

    // check that ctxId is clamped to A-Za-z0-9_-
    for (PRUint32 i = 0; i < ctxId.Length(); i++) {
      if ((ctxId[i] < 'A' || ctxId[i] > 'Z') &&
          (ctxId[i] < 'a' || ctxId[i] > 'z') &&
          (ctxId[i] < '0' || ctxId[i] > '9') &&
          (ctxId[i] != '-') &&
          (ctxId[i] != '_'))
      {
        return NS_ERROR_INVALID_ARG;
      }
    }

    nsCString ctxString("@mozilla.org/content/canvas-rendering-context;1?id=");
    ctxString.Append(ctxId);

    mCurrentContext = do_CreateInstance(nsPromiseFlatCString(ctxString).get(), &rv);
    if (rv == NS_ERROR_OUT_OF_MEMORY)
      return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(rv))
      return NS_ERROR_INVALID_ARG;

    rv = mCurrentContext->SetCanvasElement(this);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = UpdateImageContainer(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentContextId.Assign(aContextId);
  } else if (!mCurrentContextId.Equals(aContextId)) {
    //XXX eventually allow for more than one active context on a given canvas
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}

nsresult
nsHTMLCanvasElement::UpdateImageContainer(PRBool forceCreate)
{
  nsresult rv = NS_OK;

  // don't create if we don't already have one,
  // and no frame or context has asked for one.
  if (!forceCreate && !mImageFrame)
    return NS_OK;

  nsIntSize sz = GetWidthHeight();
  PRInt32 w = 0, h = 0;

  if (mImageFrame) {
    mImageFrame->GetWidth(&w);
    mImageFrame->GetHeight(&h);
  }

  if (sz.width != w || sz.height != h) {
    mImageContainer = do_CreateInstance("@mozilla.org/image/container;1");
    mImageContainer->Init(sz.width, sz.height, nsnull);

    mImageFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (!mImageFrame)
      return NS_ERROR_FAILURE;

#ifdef XP_WIN
    rv = mImageFrame->Init(0, 0, sz.width, sz.height, gfxIFormats::BGR_A8, 24);
#else
    rv = mImageFrame->Init(0, 0, sz.width, sz.height, gfxIFormats::RGB_A8, 24);
#endif
    NS_ENSURE_SUCCESS(rv, rv);

    mImageContainer->AppendFrame(mImageFrame);
  }

  return UpdateContext();
}

nsresult
nsHTMLCanvasElement::UpdateContext()
{
  if (mCurrentContext)
    return mCurrentContext->SetTargetImageFrame(mImageFrame);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetCanvasImageContainer(imgIContainer **aImageContainer)
{
  nsresult rv;

  if (!mImageContainer) {
    rv = UpdateImageContainer(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aImageContainer = mImageContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetPrimaryCanvasFrame(nsIFrame **aFrame)
{
  *aFrame = GetPrimaryFrame(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::UpdateImageFrame()
{
  if (mCurrentContext)
    return mCurrentContext->UpdateImageFrame();

  return NS_OK;
}
