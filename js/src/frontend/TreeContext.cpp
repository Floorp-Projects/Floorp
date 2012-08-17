/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNode.h"
#include "frontend/TreeContext.h"

#include "jsatominlines.h"

#include "frontend/ParseNode-inl.h"
#include "frontend/TreeContext-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::frontend;

bool
frontend::GenerateBlockId(TreeContext *tc, uint32_t &blockid)
{
    if (tc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(tc->sc->context, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "program");
        return false;
    }
    JS_ASSERT(tc->blockidGen < JS_BIT(20));
    blockid = tc->blockidGen++;
    return true;
}

/* See comment on member function declaration. */
bool
TreeContext::define(JSContext *cx, PropertyName *name, ParseNode *pn, Definition::Kind kind)
{
    JS_ASSERT(!pn->isUsed());
    JS_ASSERT_IF(pn->isDefn(), pn->isPlaceholder());

    Definition *prevDef = NULL;
    if (kind == Definition::LET)
        prevDef = decls_.lookupFirst(name);
    else
        JS_ASSERT(!decls_.lookupFirst(name));

    if (!prevDef)
        prevDef = lexdeps.lookupDefn(name);

    if (prevDef) {
        ParseNode **pnup = &prevDef->dn_uses;
        ParseNode *pnu;
        unsigned start = (kind == Definition::LET) ? pn->pn_blockid : bodyid;

        while ((pnu = *pnup) != NULL && pnu->pn_blockid >= start) {
            JS_ASSERT(pnu->pn_blockid >= bodyid);
            JS_ASSERT(pnu->isUsed());
            pnu->pn_lexdef = (Definition *) pn;
            pn->pn_dflags |= pnu->pn_dflags & PND_USE2DEF_FLAGS;
            pnup = &pnu->pn_link;
        }

        if (!pnu || pnu != prevDef->dn_uses) {
            *pnup = pn->dn_uses;
            pn->dn_uses = prevDef->dn_uses;
            prevDef->dn_uses = pnu;

            if (!pnu && prevDef->isPlaceholder())
                lexdeps->remove(name);
        }

        pn->pn_dflags |= prevDef->pn_dflags & PND_CLOSED;
    }

    JS_ASSERT_IF(kind != Definition::LET, !lexdeps->lookup(name));
    pn->setDefn(true);
    pn->pn_dflags &= ~PND_PLACEHOLDER;
    if (kind == Definition::CONST)
        pn->pn_dflags |= PND_CONST;

    Definition *dn = (Definition *)pn;
    switch (kind) {
      case Definition::ARG:
        JS_ASSERT(sc->inFunction());
        dn->setOp(JSOP_GETARG);
        dn->pn_dflags |= PND_BOUND;
        if (!dn->pn_cookie.set(cx, staticLevel, args_.length()))
            return false;
        if (!args_.append(dn))
            return false;
        if (name == cx->runtime->atomState.emptyAtom)
            break;
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::CONST:
      case Definition::VAR:
        if (sc->inFunction()) {
            dn->setOp(JSOP_GETLOCAL);
            dn->pn_dflags |= PND_BOUND;
            if (!dn->pn_cookie.set(cx, staticLevel, vars_.length()))
                return false;
            if (!vars_.append(dn))
                return false;
        }
        if (!decls_.addUnique(name, dn))
            return false;
        break;

      case Definition::LET:
        dn->setOp(JSOP_GETLOCAL);
        dn->pn_dflags |= (PND_LET | PND_BOUND);
        JS_ASSERT(dn->pn_cookie.level() == staticLevel); /* see BindLet */
        if (!decls_.addShadow(name, dn))
            return false;
        break;

      case Definition::PLACEHOLDER:
      case Definition::NAMED_LAMBDA:
        JS_NOT_REACHED("unexpected kind");
        break;
    }

    return true;
}

void
TreeContext::prepareToAddDuplicateArg(Definition *prevDecl)
{
    JS_ASSERT(prevDecl->kind() == Definition::ARG);
    JS_ASSERT(decls_.lookupFirst(prevDecl->name()) == prevDecl);
    JS_ASSERT(!prevDecl->isClosed());
    decls_.remove(prevDecl->name());
}

void
TreeContext::updateDecl(JSAtom *atom, ParseNode *pn)
{
    Definition *oldDecl = decls_.lookupFirst(atom);

    pn->setDefn(true);
    Definition *newDecl = (Definition *)pn;
    decls_.updateFirst(atom, newDecl);

    if (!sc->inFunction()) {
        JS_ASSERT(newDecl->isFreeVar());
        return;
    }

    JS_ASSERT(oldDecl->isBound());
    JS_ASSERT(!oldDecl->pn_cookie.isFree());
    newDecl->pn_cookie = oldDecl->pn_cookie;
    newDecl->pn_dflags |= PND_BOUND;
    if (JOF_OPTYPE(oldDecl->getOp()) == JOF_QARG) {
        newDecl->setOp(JSOP_GETARG);
        JS_ASSERT(args_[oldDecl->pn_cookie.slot()] == oldDecl);
        args_[oldDecl->pn_cookie.slot()] = newDecl;
    } else {
        JS_ASSERT(JOF_OPTYPE(oldDecl->getOp()) == JOF_LOCAL);
        newDecl->setOp(JSOP_GETLOCAL);
        JS_ASSERT(vars_[oldDecl->pn_cookie.slot()] == oldDecl);
        vars_[oldDecl->pn_cookie.slot()] = newDecl;
    }
}

void
TreeContext::popLetDecl(JSAtom *atom)
{
    JS_ASSERT(decls_.lookupFirst(atom)->isLet());
    decls_.remove(atom);
}

void
AppendPackedBindings(const TreeContext *tc, const DeclVector &vec, Binding *dst)
{
    for (unsigned i = 0; i < vec.length(); ++i, ++dst) {
        Definition *dn = vec[i];
        PropertyName *name = dn->name();

        BindingKind kind;
        switch (dn->kind()) {
          case Definition::VAR:
            kind = VARIABLE;
            break;
          case Definition::CONST:
            kind = CONSTANT;
            break;
          case Definition::ARG:
            kind = ARGUMENT;
            break;
          case Definition::LET:
          case Definition::NAMED_LAMBDA:
          case Definition::PLACEHOLDER:
            JS_NOT_REACHED("unexpected dn->kind");
        }

        /*
         * Bindings::init does not check for duplicates so we must ensure that
         * only one binding with a given name is marked aliased. tc->decls
         * maintains the canonical definition for each name, so use that.
         */
        JS_ASSERT_IF(dn->isClosed(), tc->decls().lookupFirst(name) == dn);
        bool aliased = dn->isClosed() ||
                       (tc->sc->bindingsAccessedDynamically() &&
                        tc->decls().lookupFirst(name) == dn);

        *dst = Binding(name, kind, aliased);
    }
}

bool
TreeContext::generateFunctionBindings(JSContext *cx, Bindings *bindings) const
{
    JS_ASSERT(sc->inFunction());

    unsigned count = args_.length() + vars_.length();
    Binding *packedBindings = cx->tempLifoAlloc().newArrayUninitialized<Binding>(count);
    if (!packedBindings) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    AppendPackedBindings(this, args_, packedBindings);
    AppendPackedBindings(this, vars_, packedBindings + args_.length());

    if (!bindings->initWithTemporaryStorage(cx, args_.length(), vars_.length(), packedBindings))
        return false;

    if (bindings->hasAnyAliasedBindings() || sc->funHasExtensibleScope())
        sc->fun()->flags |= JSFUN_HEAVYWEIGHT;

    return true;
}
