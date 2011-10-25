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

#ifndef BytecodeEmitter_inl_h__
#define BytecodeEmitter_inl_h__

#include "frontend/ParseNode.h"
#include "frontend/TokenStream.h"

namespace js {

inline
TreeContext::TreeContext(Parser *prs)
  : flags(0), bodyid(0), blockidGen(0), parenDepth(0), yieldCount(0), argumentsCount(0),
    topStmt(NULL), topScopeStmt(NULL), blockChainBox(NULL), blockNode(NULL),
    decls(prs->context), parser(prs), yieldNode(NULL), argumentsNode(NULL), scopeChain_(NULL),
    lexdeps(prs->context), parent(prs->tc), staticLevel(0), funbox(NULL), functionList(NULL),
    innermostWith(NULL), bindings(prs->context), sharpSlotBase(-1)
{
    prs->tc = this;
}

/*
 * For functions the tree context is constructed and destructed a second
 * time during code generation. To avoid a redundant stats update in such
 * cases, we store uint16(-1) in maxScopeDepth.
 */
inline
TreeContext::~TreeContext()
{
    parser->tc = this->parent;
}

} /* namespace js */

#endif /* BytecodeEmitter_inl_h__ */
