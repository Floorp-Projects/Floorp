/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines enum values for all of the DOM objects which have
 * an entry in nsDOMClassInfo.
 */

#ifndef nsDOMClassInfoID_h__
#define nsDOMClassInfoID_h__

#include "nsIXPCScriptable.h"

enum nsDOMClassInfoID
{
  eDOMClassInfo_DOMPrototype_id,
  eDOMClassInfo_DOMConstructor_id,

  // CSS classes
  eDOMClassInfo_CSSStyleRule_id,
  eDOMClassInfo_CSSImportRule_id,
  eDOMClassInfo_CSSMediaRule_id,
  eDOMClassInfo_CSSNameSpaceRule_id,

  // XUL classes
#ifdef MOZ_XUL
  eDOMClassInfo_XULCommandDispatcher_id,
#endif
  eDOMClassInfo_XULControllers_id,
#ifdef MOZ_XUL
  eDOMClassInfo_TreeSelection_id,
  eDOMClassInfo_TreeContentView_id,
#endif

#ifdef MOZ_XUL
  eDOMClassInfo_XULTemplateBuilder_id,
  eDOMClassInfo_XULTreeBuilder_id,
#endif

  eDOMClassInfo_CSSMozDocumentRule_id,
  eDOMClassInfo_CSSSupportsRule_id,

  // @font-face in CSS
  eDOMClassInfo_CSSFontFaceRule_id,

  eDOMClassInfo_ContentFrameMessageManager_id,
  eDOMClassInfo_ContentProcessMessageManager_id,
  eDOMClassInfo_ChromeMessageBroadcaster_id,
  eDOMClassInfo_ChromeMessageSender_id,

  eDOMClassInfo_CSSKeyframeRule_id,
  eDOMClassInfo_CSSKeyframesRule_id,

  // @counter-style in CSS
  eDOMClassInfo_CSSCounterStyleRule_id,

  eDOMClassInfo_CSSPageRule_id,

  eDOMClassInfo_CSSFontFeatureValuesRule_id,

  eDOMClassInfo_XULControlElement_id,
  eDOMClassInfo_XULLabeledControlElement_id,
  eDOMClassInfo_XULButtonElement_id,
  eDOMClassInfo_XULCheckboxElement_id,
  eDOMClassInfo_XULPopupElement_id,

  // This one better be the last one in this list
  eDOMClassInfoIDCount
};

/**
 * nsIClassInfo helper macros
 */

#ifdef MOZILLA_INTERNAL_API

class nsIClassInfo;
class nsXPCClassInfo;

extern nsIClassInfo*
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID);

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                          \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                                \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                              \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(_class, condition)   \
  if ((condition) &&                                                          \
      (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                               \
       aIID.Equals(NS_GET_IID(nsXPCClassInfo)))) {                            \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#endif // MOZILLA_INTERNAL_API

#endif // nsDOMClassInfoID_h__
