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
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsICanvasElement.h"
#include "nsIRenderingContext.h"

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
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLCanvasElement
  NS_DECL_NSIDOMHTMLCANVASELEMENT

  // nsICanvasElement
  NS_IMETHOD GetPrimaryCanvasFrame(nsIFrame **aFrame);
  NS_IMETHOD GetSize(PRUint32 *width, PRUint32 *height);
  NS_IMETHOD RenderContexts(gfxContext *ctx);
  virtual PRBool IsWriteOnly();
  virtual void SetWriteOnly();
  NS_IMETHOD InvalidateFrame ();
  NS_IMETHOD InvalidateFrameSubrect (const nsRect& damageRect);
  virtual PRInt32 CountContexts();
  virtual nsICanvasRenderingContextInternal *GetContextAtIndex (PRInt32 index);

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
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

protected:
  nsIntSize GetWidthHeight();
  PRBool GetIsOpaque();

  nsresult UpdateContext();
  nsresult ToDataURLImpl(const nsAString& aMimeType,
                         const nsAString& aEncoderOptions,
                         nsAString& aDataURL);

  nsString mCurrentContextId;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
  
public:
  // Record whether this canvas should be write-only or not.
  // We set this when script paints an image from a different origin.
  // We also transitively set it when script paints a canvas which
  // is itself write-only.
  PRPackedBool             mWriteOnly;
};

nsGenericHTMLElement*
NS_NewHTMLCanvasElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  return new nsHTMLCanvasElement(aNodeInfo);
}

nsHTMLCanvasElement::nsHTMLCanvasElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mWriteOnly(PR_FALSE)
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

NS_INTERFACE_TABLE_HEAD(nsHTMLCanvasElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLCanvasElement,
                                   nsIDOMHTMLCanvasElement,
                                   nsICanvasElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLCanvasElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLCanvasElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLCanvasElement)

nsIntSize
nsHTMLCanvasElement::GetWidthHeight()
{
  nsIntSize size(0,0);
  const nsAttrValue* value;

  if ((value = GetParsedAttr(nsGkAtoms::width)) &&
      value->Type() == nsAttrValue::eInteger)
  {
      size.width = value->GetIntegerValue();
  }

  if ((value = GetParsedAttr(nsGkAtoms::height)) &&
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

PRBool
nsHTMLCanvasElement::GetIsOpaque()
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque);
}

NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Width, width, DEFAULT_CANVAS_WIDTH)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Height, height, DEFAULT_CANVAS_HEIGHT)
NS_IMPL_BOOL_ATTR(nsHTMLCanvasElement, MozOpaque, moz_opaque)

nsresult
nsHTMLCanvasElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                              aNotify);
  if (NS_SUCCEEDED(rv) && mCurrentContext &&
      (aName == nsGkAtoms::width || aName == nsGkAtoms::height || aName == nsGkAtoms::moz_opaque))
  {
    rv = UpdateContext();
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
  if (aAttribute == nsGkAtoms::width ||
      aAttribute == nsGkAtoms::height)
  {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  } else if (aAttribute == nsGkAtoms::moz_opaque)
  {
    NS_UpdateHint(retval, NS_STYLE_HINT_VISUAL);
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
  { &nsGkAtoms::hspace },
  { &nsGkAtoms::vspace },
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
nsHTMLCanvasElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None)
  {
    if ((aAttribute == nsGkAtoms::width) ||
        (aAttribute == nsGkAtoms::height))
    {
      return aResult.ParseIntWithBounds(aValue, 0);
    }

    if (ParseImageAttribute(aAttribute, aValue, aResult))
    {
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}


// nsHTMLCanvasElement::toDataURL

NS_IMETHODIMP
nsHTMLCanvasElement::ToDataURL(nsAString& aDataURL)
{
  nsresult rv;

  nsAXPCNativeCallContext *ncc = nsnull;
  rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_FAILURE;

  JSContext *ctx = nsnull;

  rv = ncc->GetJSContext(&ctx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // do a trust check if this is a write-only canvas
  // or if we're trying to use the 2-arg form
  if ((mWriteOnly || argc >= 2) && !nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // 0-arg case; convert to png
  if (argc == 0) {
    return ToDataURLImpl(NS_LITERAL_STRING("image/png"), EmptyString(), aDataURL);
  }

  JSAutoRequest ar(ctx);

  // 1-arg case; convert to given mime type
  if (argc == 1) {
    if (!JSVAL_IS_STRING(argv[0]))
      return NS_ERROR_DOM_SYNTAX_ERR;
    JSString *type = JS_ValueToString(ctx, argv[0]);
    return ToDataURLImpl (nsDependentString(reinterpret_cast<PRUnichar*>((JS_GetStringChars(type)))),
                          EmptyString(), aDataURL);
  }

  // 2-arg case; trusted only (checked above), convert to mime type with params
  if (argc == 2) {
    if (!JSVAL_IS_STRING(argv[0]) || !JSVAL_IS_STRING(argv[1]))
      return NS_ERROR_DOM_SYNTAX_ERR;

    JSString *type, *params;
    type = JS_ValueToString(ctx, argv[0]);
    params = JS_ValueToString(ctx, argv[1]);

    return ToDataURLImpl (nsDependentString(reinterpret_cast<PRUnichar*>(JS_GetStringChars(type))),
                          nsDependentString(reinterpret_cast<PRUnichar*>(JS_GetStringChars(params))),
                          aDataURL);
  }

  return NS_ERROR_DOM_SYNTAX_ERR;
}


// nsHTMLCanvasElement::toDataURLAs
//
// Native-callers only

NS_IMETHODIMP
nsHTMLCanvasElement::ToDataURLAs(const nsAString& aMimeType,
                                 const nsAString& aEncoderOptions,
                                 nsAString& aDataURL)
{
  return ToDataURLImpl(aMimeType, aEncoderOptions, aDataURL);
}

nsresult
nsHTMLCanvasElement::ToDataURLImpl(const nsAString& aMimeType,
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
  rv = context->GetInputStream(nsPromiseFlatCString(aMimeType8).get(),
                               nsPromiseFlatString(aEncoderOptions).get(),
                               getter_AddRefs(imgStream));
  // XXX ERRMSG we need to report an error to developers here! (bug 329026)
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
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_INVALID_ARG;
      }
    }

    nsCString ctxString("@mozilla.org/content/canvas-rendering-context;1?id=");
    ctxString.Append(ctxId);

    mCurrentContext = do_CreateInstance(nsPromiseFlatCString(ctxString).get(), &rv);
    if (rv == NS_ERROR_OUT_OF_MEMORY)
      return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(rv))
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
      return NS_ERROR_INVALID_ARG;

    rv = mCurrentContext->SetCanvasElement(this);
    if (NS_FAILED(rv)) {
      mCurrentContext = nsnull;
      return rv;
    }

    rv = UpdateContext();
    if (NS_FAILED(rv)) {
      mCurrentContext = nsnull;
      return rv;
    }

    mCurrentContextId.Assign(aContextId);
  } else if (!mCurrentContextId.Equals(aContextId)) {
    //XXX eventually allow for more than one active context on a given canvas
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}

nsresult
nsHTMLCanvasElement::UpdateContext()
{
  nsresult rv = NS_OK;
  if (mCurrentContext) {
    nsIntSize sz = GetWidthHeight();
    rv = mCurrentContext->SetIsOpaque(GetIsOpaque());
    rv = mCurrentContext->SetDimensions(sz.width, sz.height);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetPrimaryCanvasFrame(nsIFrame **aFrame)
{
  *aFrame = GetPrimaryFrame(Flush_Frames);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetSize(PRUint32 *width, PRUint32 *height)
{
  nsIntSize sz = GetWidthHeight();
  *width = sz.width;
  *height = sz.height;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::RenderContexts(gfxContext *ctx)
{
  if (!mCurrentContext)
    return NS_OK;

  return mCurrentContext->Render(ctx);
}

PRBool
nsHTMLCanvasElement::IsWriteOnly()
{
  return mWriteOnly;
}

void
nsHTMLCanvasElement::SetWriteOnly()
{
  mWriteOnly = PR_TRUE;
}

NS_IMETHODIMP
nsHTMLCanvasElement::InvalidateFrame()
{
  nsIFrame *frame = GetPrimaryFrame(Flush_Frames);
  if (frame) {
    nsRect r = frame->GetRect();
    r.x = r.y = 0;
    frame->Invalidate(r);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::InvalidateFrameSubrect(const nsRect& damageRect)
{
  nsIFrame *frame = GetPrimaryFrame(Flush_Frames);
  if (frame) {
    frame->Invalidate(damageRect);
  }

  return NS_OK;
}

PRInt32
nsHTMLCanvasElement::CountContexts()
{
  if (mCurrentContext)
    return 1;

  return 0;
}

nsICanvasRenderingContextInternal *
nsHTMLCanvasElement::GetContextAtIndex (PRInt32 index)
{
  if (mCurrentContext && index == 0)
    return mCurrentContext.get();

  return NULL;
}
