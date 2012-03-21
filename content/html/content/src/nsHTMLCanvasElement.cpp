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

#include "nsHTMLCanvasElement.h"

#include "mozilla/Base64.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsDOMFile.h"
#include "CheckedInt.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsMathUtils.h"
#include "nsStreamUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include "nsFrameManager.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "BasicLayers.h"
#include "imgIEncoder.h"

#include "nsIWritablePropertyBag2.h"

#define DEFAULT_CANVAS_WIDTH 300
#define DEFAULT_CANVAS_HEIGHT 150

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

nsGenericHTMLElement*
NS_NewHTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        FromParser aFromParser)
{
  return new nsHTMLCanvasElement(aNodeInfo);
}

nsHTMLCanvasElement::nsHTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mWriteOnly(false)
{
}

nsHTMLCanvasElement::~nsHTMLCanvasElement()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLCanvasElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLCanvasElement,
                                                nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCurrentContext)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsHTMLCanvasElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLCanvasElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLCanvasElement, nsHTMLCanvasElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLCanvasElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLCanvasElement,
                                   nsIDOMHTMLCanvasElement,
                                   nsICanvasElementExternal)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLCanvasElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLCanvasElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLCanvasElement)

nsIntSize
nsHTMLCanvasElement::GetWidthHeight()
{
  nsIntSize size(DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT);
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

  return size;
}

NS_IMPL_UINT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Width, width, DEFAULT_CANVAS_WIDTH)
NS_IMPL_UINT_ATTR_DEFAULT_VALUE(nsHTMLCanvasElement, Height, height, DEFAULT_CANVAS_HEIGHT)
NS_IMPL_BOOL_ATTR(nsHTMLCanvasElement, MozOpaque, moz_opaque)

nsresult
nsHTMLCanvasElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             bool aNotify)
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

nsresult
nsHTMLCanvasElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    nsHTMLCanvasElement* dest = static_cast<nsHTMLCanvasElement*>(aDest);
    nsCOMPtr<nsISupports> cxt;
    dest->GetContext(NS_LITERAL_STRING("2d"), JSVAL_VOID, getter_AddRefs(cxt));
    nsCOMPtr<nsIDOMCanvasRenderingContext2D> context2d = do_QueryInterface(cxt);
    if (context2d) {
      context2d->DrawImage(const_cast<nsHTMLCanvasElement*>(this),
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0);
    }
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

bool
nsHTMLCanvasElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    return aResult.ParseNonNegativeIntValue(aValue);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}


// nsHTMLCanvasElement::toDataURL

NS_IMETHODIMP
nsHTMLCanvasElement::ToDataURL(const nsAString& aType, nsIVariant* aParams,
                               PRUint8 optional_argc, nsAString& aDataURL)
{
  // do a trust check if this is a write-only canvas
  if (mWriteOnly && !nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return ToDataURLImpl(aType, aParams, aDataURL);
}

// nsHTMLCanvasElement::mozFetchAsStream

NS_IMETHODIMP
nsHTMLCanvasElement::MozFetchAsStream(nsIInputStreamCallback *aCallback,
                                      const nsAString& aType)
{
  if (!nsContentUtils::IsCallerChrome())
    return NS_ERROR_FAILURE;

  nsresult rv;
  bool fellBackToPNG = false;
  nsCOMPtr<nsIInputStream> inputData;

  rv = ExtractData(aType, EmptyString(), getter_AddRefs(inputData), fellBackToPNG);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAsyncInputStream> asyncData = do_QueryInterface(inputData, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIThread> mainThread;
  rv = NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStreamCallback> asyncCallback;
  rv = NS_NewInputStreamReadyEvent(getter_AddRefs(asyncCallback), aCallback, mainThread);
  NS_ENSURE_SUCCESS(rv, rv);

  return asyncCallback->OnInputStreamReady(asyncData);
}

nsresult
nsHTMLCanvasElement::ExtractData(const nsAString& aType,
                                 const nsAString& aOptions,
                                 nsIInputStream** aStream,
                                 bool& aFellBackToPNG)
{
  // note that if we don't have a current context, the spec says we're
  // supposed to just return transparent black pixels of the canvas
  // dimensions.
  nsRefPtr<gfxImageSurface> emptyCanvas;
  nsIntSize size = GetWidthHeight();
  if (!mCurrentContext) {
    emptyCanvas = new gfxImageSurface(gfxIntSize(size.width, size.height), gfxASurface::ImageFormatARGB32);
    if (emptyCanvas->CairoStatus()) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  nsresult rv;

  // get image bytes
  nsCOMPtr<nsIInputStream> imgStream;
  NS_ConvertUTF16toUTF8 encoderType(aType);

 try_again:
  if (mCurrentContext) {
    rv = mCurrentContext->GetInputStream(encoderType.get(),
                                         nsPromiseFlatString(aOptions).get(),
                                         getter_AddRefs(imgStream));
  } else {
    // no context, so we have to encode the empty image we created above
    nsCString enccid("@mozilla.org/image/encoder;2?type=");
    enccid += encoderType;

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get(), &rv);
    if (NS_SUCCEEDED(rv) && encoder) {
      rv = encoder->InitFromData(emptyCanvas->Data(),
                                 size.width * size.height * 4,
                                 size.width,
                                 size.height,
                                 size.width * 4,
                                 imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                 aOptions);
      if (NS_SUCCEEDED(rv)) {
        imgStream = do_QueryInterface(encoder);
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(rv) && !aFellBackToPNG) {
    // Try image/png instead.
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    aFellBackToPNG = true;
    encoderType.AssignLiteral("image/png");
    goto try_again;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  imgStream.forget(aStream);
  return NS_OK;
}

nsresult
nsHTMLCanvasElement::ToDataURLImpl(const nsAString& aMimeType,
                                   nsIVariant* aEncoderOptions,
                                   nsAString& aDataURL)
{
  bool fallbackToPNG = false;

  nsIntSize size = GetWidthHeight();
  if (size.height == 0 || size.width == 0) {
    aDataURL = NS_LITERAL_STRING("data:,");
    return NS_OK;
  }

  nsAutoString type;
  nsresult rv = nsContentUtils::ASCIIToLower(aMimeType, type);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString params;

  // Quality parameter is only valid for the image/jpeg MIME type
  if (type.EqualsLiteral("image/jpeg")) {
    PRUint16 vartype;

    if (aEncoderOptions &&
        NS_SUCCEEDED(aEncoderOptions->GetDataType(&vartype)) &&
        vartype <= nsIDataType::VTYPE_DOUBLE) {

      double quality;
      // Quality must be between 0.0 and 1.0, inclusive
      if (NS_SUCCEEDED(aEncoderOptions->GetAsDouble(&quality)) &&
          quality >= 0.0 && quality <= 1.0) {
        params.AppendLiteral("quality=");
        params.AppendInt(NS_lround(quality * 100.0));
      }
    }
  }

  // If we haven't parsed the params check for proprietary options.
  // The proprietary option -moz-parse-options will take a image lib encoder
  // parse options string as is and pass it to the encoder.
  bool usingCustomParseOptions = false;
  if (params.Length() == 0) {
    NS_NAMED_LITERAL_STRING(mozParseOptions, "-moz-parse-options:");
    nsAutoString paramString;
    if (NS_SUCCEEDED(aEncoderOptions->GetAsAString(paramString)) && 
        StringBeginsWith(paramString, mozParseOptions)) {
      nsDependentSubstring parseOptions = Substring(paramString, 
                                                    mozParseOptions.Length(), 
                                                    paramString.Length() - 
                                                    mozParseOptions.Length());
      params.Append(parseOptions);
      usingCustomParseOptions = true;
    }
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = ExtractData(type, params, getter_AddRefs(stream), fallbackToPNG);

  // If there are unrecognized custom parse options, we should fall back to 
  // the default values for the encoder without any options at all.
  if (rv == NS_ERROR_INVALID_ARG && usingCustomParseOptions) {
    fallbackToPNG = false;
    rv = ExtractData(type, EmptyString(), getter_AddRefs(stream), fallbackToPNG);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // build data URL string
  if (fallbackToPNG)
    aDataURL = NS_LITERAL_STRING("data:image/png;base64,");
  else
    aDataURL = NS_LITERAL_STRING("data:") + type +
      NS_LITERAL_STRING(";base64,");

  PRUint32 count;
  rv = stream->Available(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  return Base64EncodeInputStream(stream, aDataURL, count, aDataURL.Length());
}

NS_IMETHODIMP
nsHTMLCanvasElement::MozGetAsFile(const nsAString& aName,
                                  const nsAString& aType,
                                  PRUint8 optional_argc,
                                  nsIDOMFile** aResult)
{
  // do a trust check if this is a write-only canvas
  if ((mWriteOnly) &&
      !nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return MozGetAsFileImpl(aName, aType, aResult);
}

nsresult
nsHTMLCanvasElement::MozGetAsFileImpl(const nsAString& aName,
                                      const nsAString& aType,
                                      nsIDOMFile** aResult)
{
  bool fallbackToPNG = false;

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = ExtractData(aType, EmptyString(), getter_AddRefs(stream),
                            fallbackToPNG);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString type(aType);
  if (fallbackToPNG) {
    type.AssignLiteral("image/png");
  }

  PRUint32 imgSize;
  rv = stream->Available(&imgSize);
  NS_ENSURE_SUCCESS(rv, rv);

  void* imgData = nsnull;
  rv = NS_ReadInputStreamToBuffer(stream, &imgData, imgSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // The DOMFile takes ownership of the buffer
  nsRefPtr<nsDOMMemoryFile> file =
    new nsDOMMemoryFile(imgData, imgSize, aName, type);

  file.forget(aResult);
  return NS_OK;
}

nsresult
nsHTMLCanvasElement::GetContextHelper(const nsAString& aContextId,
                                      bool aForceThebes,
                                      nsICanvasRenderingContextInternal **aContext)
{
  NS_ENSURE_ARG(aContext);

  NS_ConvertUTF16toUTF8 ctxId(aContextId);

  // check that ctxId is clamped to A-Za-z0-9_-
  for (PRUint32 i = 0; i < ctxId.Length(); i++) {
    if ((ctxId[i] < 'A' || ctxId[i] > 'Z') &&
        (ctxId[i] < 'a' || ctxId[i] > 'z') &&
        (ctxId[i] < '0' || ctxId[i] > '9') &&
        (ctxId[i] != '-') &&
        (ctxId[i] != '_'))
    {
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
      return NS_OK;
    }
  }

  nsCString ctxString("@mozilla.org/content/canvas-rendering-context;1?id=");
  ctxString.Append(ctxId);

  if (aForceThebes && ctxId.EqualsASCII("2d")) {
    ctxString.AssignASCII("@mozilla.org/content/2dthebes-canvas-rendering-context;1");
  }

  nsresult rv;
  nsCOMPtr<nsICanvasRenderingContextInternal> ctx =
    do_CreateInstance(ctxString.get(), &rv);
  if (rv == NS_ERROR_OUT_OF_MEMORY) {
    *aContext = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (NS_FAILED(rv)) {
    *aContext = nsnull;
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
  }

  rv = ctx->SetCanvasElement(this);
  if (NS_FAILED(rv)) {
    *aContext = nsnull;
    return rv;
  }

  ctx.forget(aContext);

  return rv;
}

NS_IMETHODIMP
nsHTMLCanvasElement::GetContext(const nsAString& aContextId,
                                const jsval& aContextOptions,
                                nsISupports **aContext)
{
  nsresult rv;

  bool forceThebes = false;

  while (mCurrentContextId.IsEmpty()) {
    rv = GetContextHelper(aContextId, forceThebes, getter_AddRefs(mCurrentContext));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mCurrentContext) {
      return NS_OK;
    }

    // Ensure that the context participates in CC.  Note that returning a
    // CC participant from QI doesn't addref.
    nsXPCOMCycleCollectionParticipant *cp = nsnull;
    CallQueryInterface(mCurrentContext, &cp);
    if (!cp) {
      mCurrentContext = nsnull;
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIWritablePropertyBag2> contextProps;
    if (!JSVAL_IS_NULL(aContextOptions) &&
        !JSVAL_IS_VOID(aContextOptions))
    {
      JSContext *cx = nsContentUtils::GetCurrentJSContext();

      // note: if any contexts end up supporting something other
      // than objects, e.g. plain strings, then we'll need to expand
      // this to know how to create nsISupportsStrings etc.
      if (JSVAL_IS_OBJECT(aContextOptions)) {
        contextProps = do_CreateInstance("@mozilla.org/hash-property-bag;1");

        JSObject *opts = JSVAL_TO_OBJECT(aContextOptions);
        JS::AutoIdArray props(cx, JS_Enumerate(cx, opts));
        for (size_t i = 0; !!props && i < props.length(); ++i) {
          jsid propid = props[i];
          jsval propname, propval;
          if (!JS_IdToValue(cx, propid, &propname) ||
              !JS_GetPropertyById(cx, opts, propid, &propval)) {
            continue;
          }

          JSString *propnameString = JS_ValueToString(cx, propname);
          nsDependentJSString pstr;
          if (!propnameString || !pstr.init(cx, propnameString)) {
            mCurrentContext = nsnull;
            return NS_ERROR_FAILURE;
          }

          if (JSVAL_IS_BOOLEAN(propval)) {
            contextProps->SetPropertyAsBool(pstr, JSVAL_TO_BOOLEAN(propval));
          } else if (JSVAL_IS_INT(propval)) {
            contextProps->SetPropertyAsInt32(pstr, JSVAL_TO_INT(propval));
          } else if (JSVAL_IS_DOUBLE(propval)) {
            contextProps->SetPropertyAsDouble(pstr, JSVAL_TO_DOUBLE(propval));
          } else if (JSVAL_IS_STRING(propval)) {
            JSString *propvalString = JS_ValueToString(cx, propval);
            nsDependentJSString vstr;
            if (!propvalString || !vstr.init(cx, propvalString)) {
              mCurrentContext = nsnull;
              return NS_ERROR_FAILURE;
            }

            contextProps->SetPropertyAsAString(pstr, vstr);
          }
        }
      }
    }

    rv = UpdateContext(contextProps);
    if (NS_FAILED(rv)) {
      if (!forceThebes) {
        // Try again with a Thebes context
        forceThebes = true;
        continue;
      }
      return rv;
    }

    mCurrentContextId.Assign(aContextId);
    break;
  }
  if (!mCurrentContextId.Equals(aContextId)) {
    //XXX eventually allow for more than one active context on a given canvas
    return NS_OK;
  }

  NS_ADDREF (*aContext = mCurrentContext);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLCanvasElement::MozGetIPCContext(const nsAString& aContextId,
                                      nsISupports **aContext)
{
  if(!nsContentUtils::IsCallerTrustedForRead()) {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // We only support 2d shmem contexts for now.
  if (!aContextId.Equals(NS_LITERAL_STRING("2d")))
    return NS_ERROR_INVALID_ARG;

  if (mCurrentContextId.IsEmpty()) {
    nsresult rv = GetContextHelper(aContextId, false, getter_AddRefs(mCurrentContext));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mCurrentContext) {
      return NS_OK;
    }

    mCurrentContext->SetIsIPC(true);

    rv = UpdateContext();
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
nsHTMLCanvasElement::UpdateContext(nsIPropertyBag *aNewContextOptions)
{
  if (!mCurrentContext)
    return NS_OK;

  nsIntSize sz = GetWidthHeight();

  nsresult rv = mCurrentContext->SetIsOpaque(GetIsOpaque());
  if (NS_FAILED(rv)) {
    mCurrentContext = nsnull;
    mCurrentContextId.Truncate();
    return rv;
  }

  rv = mCurrentContext->SetContextOptions(aNewContextOptions);
  if (NS_FAILED(rv)) {
    mCurrentContext = nsnull;
    mCurrentContextId.Truncate();
    return rv;
  }

  rv = mCurrentContext->SetDimensions(sz.width, sz.height);
  if (NS_FAILED(rv)) {
    mCurrentContext = nsnull;
    mCurrentContextId.Truncate();
    return rv;
  }

  return rv;
}

nsIFrame *
nsHTMLCanvasElement::GetPrimaryCanvasFrame()
{
  return GetPrimaryFrame(Flush_Frames);
}

nsIntSize
nsHTMLCanvasElement::GetSize()
{
  return GetWidthHeight();
}

bool
nsHTMLCanvasElement::IsWriteOnly()
{
  return mWriteOnly;
}

void
nsHTMLCanvasElement::SetWriteOnly()
{
  mWriteOnly = true;
}

void
nsHTMLCanvasElement::InvalidateCanvasContent(const gfxRect* damageRect)
{
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame *frame = GetPrimaryFrame();
  if (!frame)
    return;

  frame->MarkLayersActive(nsChangeHint(0));

  nsRect invalRect;
  nsRect contentArea = frame->GetContentRect();
  if (damageRect) {
    nsIntSize size = GetWidthHeight();
    if (size.width != 0 && size.height != 0) {

      // damageRect and size are in CSS pixels; contentArea is in appunits
      // We want a rect in appunits; so avoid doing pixels-to-appunits and
      // vice versa conversion here.
      gfxRect realRect(*damageRect);
      realRect.Scale(contentArea.width / gfxFloat(size.width),
                     contentArea.height / gfxFloat(size.height));
      realRect.RoundOut();

      // then make it a nsRect
      invalRect = nsRect(realRect.X(), realRect.Y(),
                         realRect.Width(), realRect.Height());

      invalRect = invalRect.Intersect(nsRect(nsPoint(0,0), contentArea.Size()));
    }
  } else {
    invalRect = nsRect(nsPoint(0, 0), contentArea.Size());
  }
  invalRect.MoveBy(contentArea.TopLeft() - frame->GetPosition());

  Layer* layer = frame->InvalidateLayer(invalRect, nsDisplayItem::TYPE_CANVAS);
  if (layer) {
    static_cast<CanvasLayer*>(layer)->Updated();
  }
}

void
nsHTMLCanvasElement::InvalidateCanvas()
{
  // We don't need to flush anything here; if there's no frame or if
  // we plan to reframe we don't need to invalidate it anyway.
  nsIFrame *frame = GetPrimaryFrame();
  if (!frame)
    return;

  frame->Invalidate(frame->GetContentRect() - frame->GetPosition());
}

PRInt32
nsHTMLCanvasElement::CountContexts()
{
  if (mCurrentContext)
    return 1;

  return 0;
}

nsICanvasRenderingContextInternal *
nsHTMLCanvasElement::GetContextAtIndex(PRInt32 index)
{
  if (mCurrentContext && index == 0)
    return mCurrentContext;

  return NULL;
}

bool
nsHTMLCanvasElement::GetIsOpaque()
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::moz_opaque);
}

already_AddRefed<CanvasLayer>
nsHTMLCanvasElement::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                    CanvasLayer *aOldLayer,
                                    LayerManager *aManager)
{
  if (!mCurrentContext)
    return nsnull;

  return mCurrentContext->GetCanvasLayer(aBuilder, aOldLayer, aManager);
}

bool
nsHTMLCanvasElement::ShouldForceInactiveLayer(LayerManager *aManager)
{
  return !mCurrentContext || mCurrentContext->ShouldForceInactiveLayer(aManager);
}

void
nsHTMLCanvasElement::MarkContextClean()
{
  if (!mCurrentContext)
    return;

  mCurrentContext->MarkContextClean();
}

NS_IMETHODIMP_(nsIntSize)
nsHTMLCanvasElement::GetSizeExternal()
{
  return GetWidthHeight();
}

NS_IMETHODIMP
nsHTMLCanvasElement::RenderContextsExternal(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter)
{
  if (!mCurrentContext)
    return NS_OK;

  return mCurrentContext->Render(aContext, aFilter);
}

nsresult NS_NewCanvasRenderingContext2DThebes(nsIDOMCanvasRenderingContext2D** aResult);
nsresult NS_NewCanvasRenderingContext2DAzure(nsIDOMCanvasRenderingContext2D** aResult);

nsresult
NS_NewCanvasRenderingContext2D(nsIDOMCanvasRenderingContext2D** aResult)
{
  Telemetry::Accumulate(Telemetry::CANVAS_2D_USED, 1);
  if (Preferences::GetBool("gfx.canvas.azure.enabled", false)) {
    nsresult rv = NS_NewCanvasRenderingContext2DAzure(aResult);
    // If Azure fails, fall back to a classic canvas.
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }
  }

  return NS_NewCanvasRenderingContext2DThebes(aResult);
}
