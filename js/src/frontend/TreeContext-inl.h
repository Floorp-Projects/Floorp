/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef TreeContext_inl_h__
#define TreeContext_inl_h__

#include "frontend/Parser.h"
#include "frontend/TreeContext.h"

#include "frontend/ParseMaps-inl.h"

namespace js {

inline
SharedContext::SharedContext(JSContext *cx, bool inFunction)
  : context(cx),
    bodyid(0),
    blockidGen(0),
    topStmt(NULL),
    topScopeStmt(NULL),
    blockChain(cx),
    fun_(cx),
    scopeChain_(cx),
    staticLevel(0),
    funbox(NULL),
    functionList(NULL),
    bindings(cx),
    bindingsRoot(cx, &bindings),
    inFunction(inFunction),
    inForInit(false),
    cxFlags(cx)
{
}

inline unsigned
SharedContext::blockid()
{
    return topStmt ? topStmt->blockid : bodyid;
}

inline bool
SharedContext::atBodyLevel()
{
    return !topStmt || (topStmt->flags & SIF_BODY_BLOCK);
}

inline bool
SharedContext::needStrictChecks() {
    return context->hasStrictOption() || inStrictMode();
}

inline unsigned
SharedContext::argumentsLocalSlot() const {
    PropertyName *arguments = context->runtime->atomState.argumentsAtom;
    unsigned slot;
    DebugOnly<BindingKind> kind = bindings.lookup(context, arguments, &slot);
    JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
    return slot;
}

inline
TreeContext::TreeContext(Parser *prs, SharedContext *sc)
  : sc(sc),
    parenDepth(0),
    yieldCount(0),
    blockNode(NULL),
    decls(prs->context),
    yieldNode(NULL),
    argumentsNode(NULL),
    parserTC(&prs->tc),
    lexdeps(prs->context),
    parent(prs->tc),
    innermostWith(NULL),
    funcStmts(NULL),
    hasReturnExpr(false),
    hasReturnVoid(false),
    inDeclDestructuring(false)
{
    prs->tc = this;
}

inline bool
TreeContext::init(JSContext *cx)
{
    return decls.init() && lexdeps.ensureMap(sc->context);
}

// For functions the tree context is constructed and destructed a second
// time during code generation. To avoid a redundant stats update in such
// cases, we store UINT16_MAX in maxScopeDepth.
inline
TreeContext::~TreeContext()
{
    // |*parserTC| pointed to this object.  Now that this object is about to
    // die, make |*parserTC| point to this object's parent.
    JS_ASSERT(*parserTC == this);
    *parserTC = this->parent;
    sc->context->delete_(funcStmts);
}

} // namespace js

#endif // TreeContext_inl_h__
