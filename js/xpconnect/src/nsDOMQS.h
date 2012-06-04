/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMQS_h__
#define nsDOMQS_h__

#include "mozilla/dom/ImageData.h"
#include "nsDOMClassInfoID.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLCanvasElement.h"
#include "nsHTMLImageElement.h"
#include "nsHTMLVideoElement.h"
#include "nsHTMLDocument.h"
#include "nsICSSDeclaration.h"
#include "nsSVGStylableElement.h"

#define DEFINE_UNWRAP_CAST(_interface, _base, _bit)                           \
template <>                                                                   \
inline JSBool                                                                 \
xpc_qsUnwrapThis<_interface>(JSContext *cx,                                   \
                             JSObject *obj,                                   \
                             _interface **ppThis,                             \
                             nsISupports **pThisRef,                          \
                             jsval *pThisVal,                                 \
                             XPCLazyCallContext *lccx,                        \
                             bool failureFatal)                               \
{                                                                             \
    nsresult rv;                                                              \
    nsISupports *native = castNativeFromWrapper(cx, obj, _bit,                \
                                                pThisRef, pThisVal, lccx,     \
                                                &rv);                         \
    *ppThis = NULL;  /* avoids uninitialized warnings in callers */           \
    if (failureFatal && !native)                                              \
        return xpc_qsThrow(cx, rv);                                           \
    *ppThis = static_cast<_interface*>(static_cast<_base*>(native));          \
    return true;                                                              \
}                                                                             \
                                                                              \
template <>                                                                   \
inline nsresult                                                               \
xpc_qsUnwrapArg<_interface>(JSContext *cx,                                    \
                            jsval v,                                          \
                            _interface **ppArg,                               \
                            nsISupports **ppArgRef,                           \
                            jsval *vp)                                        \
{                                                                             \
    nsresult rv;                                                              \
    nsISupports *native = castNativeArgFromWrapper(cx, v, _bit, ppArgRef, vp, \
                                                   &rv);                      \
    if (NS_SUCCEEDED(rv))                                                     \
        *ppArg = static_cast<_interface*>(static_cast<_base*>(native));       \
    return rv;                                                                \
}

#undef DOMCI_CASTABLE_INTERFACE

#undef DOMCI_CASTABLE_INTERFACE
#define DOMCI_CASTABLE_INTERFACE(_interface, _base, _bit, _extra)             \
  DEFINE_UNWRAP_CAST(_interface, _base, _bit)

DOMCI_CASTABLE_INTERFACES(unused)

#undef DOMCI_CASTABLE_INTERFACE

// Ideally we'd just add nsGenericElement to the castable interfaces, but for
// now nsDocumentFragment inherits from nsGenericElement (even though it's not
// an Element) so we have to special-case nsGenericElement and use
// nsIContent::IsElement().
// FIXME: bug 563659.
inline JSBool
castToElement(nsIContent *content, jsval val, nsGenericElement **ppInterface,
              jsval *pVal)
{
    if (!content->IsElement())
        return false;
    *ppInterface = static_cast<nsGenericElement*>(content->AsElement());
    *pVal = val;
    return true;
}

template <>
inline JSBool
xpc_qsUnwrapThis<nsGenericElement>(JSContext *cx,
                                   JSObject *obj,
                                   nsGenericElement **ppThis,
                                   nsISupports **pThisRef,
                                   jsval *pThisVal,
                                   XPCLazyCallContext *lccx,
                                   bool failureFatal)
{
    nsIContent *content;
    jsval val;
    JSBool ok = xpc_qsUnwrapThis<nsIContent>(cx, obj, &content,
                                             pThisRef, &val, lccx,
                                             failureFatal);
    if (ok) {
        if (failureFatal || content)
          ok = castToElement(content, val, ppThis, pThisVal);
        if (failureFatal && !ok)
            xpc_qsThrow(cx, NS_ERROR_XPC_BAD_OP_ON_WN_PROTO);
    }

    if (!failureFatal && (!ok || !content)) {
      ok = true;
      *ppThis = nsnull;
    }

    return ok;
}

template <>
inline nsresult
xpc_qsUnwrapArg<nsGenericElement>(JSContext *cx,
                                  jsval v,
                                  nsGenericElement **ppArg,
                                  nsISupports **ppArgRef,
                                  jsval *vp)
{
    nsIContent *content;
    jsval val;
    nsresult rv = xpc_qsUnwrapArg<nsIContent>(cx, v, &content, ppArgRef, &val);
    if (NS_SUCCEEDED(rv) && !castToElement(content, val, ppArg, vp))
        rv = NS_ERROR_XPC_BAD_CONVERT_JS;
    return rv;
}

inline nsresult
xpc_qsUnwrapArg_HTMLElement(JSContext *cx,
                            jsval v,
                            nsIAtom *aTag,
                            nsIContent **ppArg,
                            nsISupports **ppArgRef,
                            jsval *vp)
{
    nsIContent *elem;
    jsval val;
    nsresult rv = xpc_qsUnwrapArg<nsIContent>(cx, v, &elem, ppArgRef, &val);
    if (NS_SUCCEEDED(rv)) {
        if (elem->IsHTML(aTag)) {
            *ppArg = elem;
            *vp = val;
        } else {
            rv = NS_ERROR_XPC_BAD_CONVERT_JS;
        }
    }
    return rv;
}

#define DEFINE_UNWRAP_CAST_HTML(_tag, _clazz)                                 \
template <>                                                                   \
inline nsresult                                                               \
xpc_qsUnwrapArg<_clazz>(JSContext *cx,                                        \
                        jsval v,                                              \
                        _clazz **ppArg,                                       \
                        nsISupports **ppArgRef,                               \
                        jsval *vp)                                            \
{                                                                             \
    nsIContent *elem;                                                         \
    nsresult rv = xpc_qsUnwrapArg_HTMLElement(cx, v, nsGkAtoms::_tag, &elem,  \
                                              ppArgRef, vp);                  \
    if (NS_SUCCEEDED(rv))                                                     \
        *ppArg = static_cast<_clazz*>(elem);                                  \
    return rv;                                                                \
}                                                                             \
                                                                              \
template <>                                                                   \
inline nsresult                                                               \
xpc_qsUnwrapArg<_clazz>(JSContext *cx, jsval v, _clazz **ppArg,               \
                        _clazz **ppArgRef, jsval *vp)                         \
{                                                                             \
    nsISupports* argRef = static_cast<nsIContent*>(*ppArgRef);                \
    nsresult rv = xpc_qsUnwrapArg<_clazz>(cx, v, ppArg, &argRef, vp);         \
    *ppArgRef = static_cast<_clazz*>(static_cast<nsIContent*>(argRef));       \
    return rv;                                                                \
}

DEFINE_UNWRAP_CAST_HTML(canvas, nsHTMLCanvasElement)
DEFINE_UNWRAP_CAST_HTML(img, nsHTMLImageElement)
DEFINE_UNWRAP_CAST_HTML(video, nsHTMLVideoElement)

template <>
inline nsresult
xpc_qsUnwrapArg<mozilla::dom::ImageData>(JSContext *cx, jsval v,
                                         mozilla::dom::ImageData **ppArg,
                                         mozilla::dom::ImageData **ppArgRef,
                                         jsval *vp)
{
    nsIDOMImageData* arg;
    nsIDOMImageData* argRef;
    nsresult rv = xpc_qsUnwrapArg<nsIDOMImageData>(cx, v, &arg, &argRef, vp);
    if (NS_SUCCEEDED(rv)) {
        *ppArg = static_cast<mozilla::dom::ImageData*>(arg);
        *ppArgRef = static_cast<mozilla::dom::ImageData*>(argRef);
    }
    return rv;
}

inline nsISupports*
ToSupports(nsContentList *p)
{
    return static_cast<nsINodeList*>(p);
}

inline nsISupports*
ToCanonicalSupports(nsINode* p)
{
    return p;
}

inline nsISupports*
ToSupports(nsINode* p)
{
  return p;
}

inline nsISupports*
ToCanonicalSupports(nsContentList *p)
{
    return static_cast<nsINodeList*>(p);
}

#endif /* nsDOMQS_h__ */
