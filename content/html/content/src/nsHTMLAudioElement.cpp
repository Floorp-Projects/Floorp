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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#include "nsIDOMHTMLAudioElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLAudioElement.h"
#include "nsGenericHTMLElement.h"
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
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jstypedarray.h"
#include "nsJSUtils.h"

#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMProgressEvent.h"

using namespace mozilla::dom;

nsGenericHTMLElement*
NS_NewHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                       FromParser aFromParser)
{
  /*
   * nsHTMLAudioElement's will be created without a nsINodeInfo passed in
   * if someone says "var audio = new Audio();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nsnull);

    nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::audio, nsnull,
                                                   kNameSpaceID_XHTML,
                                                   nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, nsnull);
  }

  return new nsHTMLAudioElement(nodeInfo.forget(), aFromParser);
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAudioElement, nsHTMLMediaElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLAudioElement, nsHTMLMediaElement)

DOMCI_NODE_DATA(HTMLAudioElement, nsHTMLAudioElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLAudioElement)
NS_HTML_CONTENT_INTERFACE_TABLE3(nsHTMLAudioElement, nsIDOMHTMLMediaElement,
                                 nsIDOMHTMLAudioElement, nsIJSNativeInitializer)
NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLAudioElement,
                                               nsHTMLMediaElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLAudioElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLAudioElement)


nsHTMLAudioElement::nsHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                       FromParser aFromParser)
  : nsHTMLMediaElement(aNodeInfo, aFromParser)
{
}

nsHTMLAudioElement::~nsHTMLAudioElement()
{
}

NS_IMETHODIMP
nsHTMLAudioElement::Initialize(nsISupports* aOwner, JSContext* aContext,
                               JSObject *aObj, PRUint32 argc, jsval *argv)
{
  // Audio elements created using "new Audio(...)" should have
  // 'preload' set to 'auto' (since the script must intend to
  // play the audio)
  nsresult rv = SetAttr(kNameSpaceID_None, nsGkAtoms::preload,
                        NS_LITERAL_STRING("auto"), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  if (argc <= 0) {
    // Nothing more to do here if we don't get any arguments.
    return NS_OK;
  }

  // The only (optional) argument is the url of the audio
  JSString* jsstr = JS_ValueToString(aContext, argv[0]);
  if (!jsstr)
    return NS_ERROR_FAILURE;

  nsDependentJSString str;
  if (!str.init(aContext, jsstr))
    return NS_ERROR_FAILURE;

  rv = SetAttr(kNameSpaceID_None, nsGkAtoms::src, str, PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // We have been specified with a src URL. Begin a load.
  QueueSelectResourceTask();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAudioElement::MozSetup(PRUint32 aChannels, PRUint32 aRate)
{
  // If there is already a src provided, don't setup another stream
  if (mDecoder) {
    return NS_ERROR_FAILURE;
  }

  // MozWriteAudio divides by mChannels, so validate now.
  if (0 == aChannels) {
    return NS_ERROR_FAILURE;
  }

  if (mAudioStream) {
    mAudioStream->Shutdown();
  }

  mAudioStream = nsAudioStream::AllocateStream();
  nsresult rv = mAudioStream->Init(aChannels, aRate,
                                   nsAudioStream::FORMAT_FLOAT32);
  if (NS_FAILED(rv)) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
    return rv;
  }

  MetadataLoaded(aChannels, aRate);
  mAudioStream->SetVolume(mVolume);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAudioElement::MozWriteAudio(const jsval &aData, JSContext *aCx, PRUint32 *aRetVal)
{
  if (!mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (JSVAL_IS_PRIMITIVE(aData)) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  JSObject *darray = JSVAL_TO_OBJECT(aData);
  js::AutoValueRooter tsrc_tvr(aCx);
  JSObject *tsrc = NULL;

  // Allow either Float32Array or plain JS Array
  if (darray->getClass() == &js::TypedArray::fastClasses[js::TypedArray::TYPE_FLOAT32])
  {
    tsrc = js::TypedArray::getTypedArray(darray);
  } else if (JS_IsArrayObject(aCx, darray)) {
    JSObject *nobj = js_CreateTypedArrayWithArray(aCx, js::TypedArray::TYPE_FLOAT32, darray);
    if (!nobj) {
      return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }
    *tsrc_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
    tsrc = js::TypedArray::getTypedArray(nobj);
  } else {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  PRUint32 dataLength = JS_GetTypedArrayLength(tsrc);

  // Make sure that we are going to write the correct amount of data based
  // on number of channels.
  if (dataLength % mChannels != 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Don't write more than can be written without blocking.
  PRUint32 writeLen = NS_MIN(mAudioStream->Available(), dataLength);

  nsresult rv = mAudioStream->Write(JS_GetTypedArrayData(tsrc), writeLen, PR_TRUE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Return the actual amount written.
  *aRetVal = writeLen;
  return rv;
}

NS_IMETHODIMP
nsHTMLAudioElement::MozCurrentSampleOffset(PRUint64 *aRetVal)
{
  if (!mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  *aRetVal = mAudioStream->GetSampleOffset();
  return NS_OK;
}

  
nsresult nsHTMLAudioElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
    nsCAutoString value(
#ifdef MOZ_WEBM
      "audio/webm,"
#endif
#ifdef MOZ_OGG
      "audio/ogg,"
#endif
#ifdef MOZ_WAVE
      "audio/wav,"
#endif
      "audio/*;q=0.9,"
#ifdef MOZ_OGG
      "application/ogg;q=0.7,"
#endif
      "video/*;q=0.6,*/*;q=0.5");

    return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      value,
                                      PR_FALSE);
}
