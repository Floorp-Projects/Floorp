/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMQS_h__
#define nsDOMQS_h__

#include "nsDOMClassInfoID.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLCanvasElement.h"
#include "nsHTMLFormElement.h"
#include "nsHTMLImageElement.h"
#include "nsHTMLOptionElement.h"
#include "nsHTMLOptGroupElement.h"
#include "nsHTMLVideoElement.h"
#include "nsHTMLDocument.h"
#include "nsICSSDeclaration.h"
#include "nsSVGElement.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/DocumentBinding.h"

template<class T>
struct ProtoIDAndDepth
{
    enum {
        PrototypeID = mozilla::dom::prototypes::id::_ID_Count,
        Depth = -1
    };
};

#define NEW_BINDING(_native, _id)                                             \
template<>                                                                    \
struct ProtoIDAndDepth<_native>                                               \
{                                                                             \
    enum {                                                                    \
        PrototypeID = mozilla::dom::prototypes::id::_id,                      \
        Depth = mozilla::dom::PrototypeTraits<                                \
            static_cast<mozilla::dom::prototypes::ID>(PrototypeID)>::Depth    \
    };                                                                        \
}

NEW_BINDING(mozilla::dom::EventTarget, EventTarget);
NEW_BINDING(nsINode, Node);
NEW_BINDING(mozilla::dom::Element, Element);
NEW_BINDING(nsGenericHTMLElement, HTMLElement);
NEW_BINDING(nsIDocument, Document);
NEW_BINDING(nsDocument, Document);

#define DEFINE_UNWRAP_CAST(_interface, _base, _bit)                           \
template <>                                                                   \
MOZ_ALWAYS_INLINE JSBool                                                      \
xpc_qsUnwrapThis<_interface>(JSContext *cx,                                   \
                             JSObject *obj,                                   \
                             _interface **ppThis,                             \
                             nsISupports **pThisRef,                          \
                             jsval *pThisVal,                                 \
                             XPCLazyCallContext *lccx,                        \
                             bool failureFatal)                               \
{                                                                             \
    nsresult rv;                                                              \
    nsISupports *native =                                                     \
        castNativeFromWrapper(cx, obj, _bit,                                  \
                              ProtoIDAndDepth<_interface>::PrototypeID,       \
                              ProtoIDAndDepth<_interface>::Depth,             \
                              pThisRef, pThisVal, lccx, &rv);                 \
    *ppThis = NULL;  /* avoids uninitialized warnings in callers */           \
    if (failureFatal && !native)                                              \
        return xpc_qsThrow(cx, rv);                                           \
    *ppThis = static_cast<_interface*>(static_cast<_base*>(native));          \
    return true;                                                              \
}                                                                             \
                                                                              \
template <>                                                                   \
MOZ_ALWAYS_INLINE nsresult                                                    \
xpc_qsUnwrapArg<_interface>(JSContext *cx,                                    \
                            jsval v,                                          \
                            _interface **ppArg,                               \
                            nsISupports **ppArgRef,                           \
                            jsval *vp)                                        \
{                                                                             \
    nsresult rv;                                                              \
    nsISupports *native =                                                     \
        castNativeArgFromWrapper(cx, v, _bit,                                 \
                                 ProtoIDAndDepth<_interface>::PrototypeID,    \
                                 ProtoIDAndDepth<_interface>::Depth,          \
                                 ppArgRef, vp, &rv);                          \
    if (NS_SUCCEEDED(rv))                                                     \
        *ppArg = static_cast<_interface*>(static_cast<_base*>(native));       \
    return rv;                                                                \
}                                                                             \
template <>                                                                   \
inline nsresult                                                               \
xpc_qsUnwrapArg<_interface>(JSContext *cx,                                    \
                            jsval v,                                          \
                            _interface **ppArg,                               \
                            _interface **ppArgRef,                            \
                            jsval *vp)                                        \
{                                                                             \
    nsISupports* argRef = static_cast<_base*>(*ppArgRef);                     \
    nsresult rv = xpc_qsUnwrapArg<_interface>(cx, v, ppArg, &argRef, vp);     \
    *ppArgRef = static_cast<_interface*>(static_cast<_base*>(argRef));        \
    return rv;                                                                \
}

#undef DOMCI_CASTABLE_INTERFACE

#undef DOMCI_CASTABLE_INTERFACE
#define DOMCI_CASTABLE_INTERFACE(_interface, _base, _bit, _extra)             \
  DEFINE_UNWRAP_CAST(_interface, _base, _bit)

DOMCI_CASTABLE_INTERFACES(unused)

#undef DOMCI_CASTABLE_INTERFACE

inline nsresult
xpc_qsUnwrapArg_HTMLElement(JSContext *cx,
                            jsval v,
                            nsIAtom *aTag,
                            nsIContent **ppArg,
                            nsISupports **ppArgRef,
                            jsval *vp)
{
    nsGenericHTMLElement *elem;
    jsval val;
    nsresult rv =
        xpc_qsUnwrapArg<nsGenericHTMLElement>(cx, v, &elem, ppArgRef, &val);
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
DEFINE_UNWRAP_CAST_HTML(form, nsHTMLFormElement)
DEFINE_UNWRAP_CAST_HTML(img, nsHTMLImageElement)
DEFINE_UNWRAP_CAST_HTML(optgroup, nsHTMLOptGroupElement)
DEFINE_UNWRAP_CAST_HTML(option, nsHTMLOptionElement)
DEFINE_UNWRAP_CAST_HTML(video, nsHTMLVideoElement)

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
