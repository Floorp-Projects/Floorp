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
 * The Original Code is XPConnect code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsDOMQS_h__
#define nsDOMQS_h__

#include "nsDOMClassInfoID.h"

#define DEFINE_UNWRAP_CAST(_interface, _base, _bit)                           \
NS_SPECIALIZE_TEMPLATE                                                        \
inline JSBool                                                                 \
xpc_qsUnwrapThis<_interface>(JSContext *cx,                                   \
                             JSObject *obj,                                   \
                             JSObject *callee,                                \
                             _interface **ppThis,                             \
                             nsISupports **pThisRef,                          \
                             jsval *pThisVal,                                 \
                             XPCLazyCallContext *lccx,                        \
                             bool failureFatal)                               \
{                                                                             \
    nsresult rv;                                                              \
    nsISupports *native = castNativeFromWrapper(cx, obj, callee, _bit,        \
                                                pThisRef, pThisVal, lccx,     \
                                                &rv);                         \
    *ppThis = NULL;  /* avoids uninitialized warnings in callers */           \
    if (failureFatal && !native)                                              \
        return xpc_qsThrow(cx, rv);                                           \
    *ppThis = static_cast<_interface*>(static_cast<_base*>(native));          \
    return true;                                                              \
}                                                                             \
                                                                              \
NS_SPECIALIZE_TEMPLATE                                                        \
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

NS_SPECIALIZE_TEMPLATE
inline JSBool
xpc_qsUnwrapThis<nsGenericElement>(JSContext *cx,
                                   JSObject *obj,
                                   JSObject *callee,
                                   nsGenericElement **ppThis,
                                   nsISupports **pThisRef,
                                   jsval *pThisVal,
                                   XPCLazyCallContext *lccx,
                                   bool failureFatal)
{
    nsIContent *content;
    jsval val;
    JSBool ok = xpc_qsUnwrapThis<nsIContent>(cx, obj, callee, &content,
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

NS_SPECIALIZE_TEMPLATE
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
ToCanonicalSupports(nsContentList *p)
{
    return static_cast<nsINodeList*>(p);
}

#endif /* nsDOMQS_h__ */
