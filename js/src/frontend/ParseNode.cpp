/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNode-inl.h"

#include "mozilla/FloatingPoint.h"

#include "frontend/Parser.h"

#include "vm/JSContext-inl.h"

using namespace js;
using namespace js::frontend;

using mozilla::IsFinite;

#ifdef DEBUG
void
ParseNode::checkListConsistency()
{
    MOZ_ASSERT(isArity(PN_LIST));
    ParseNode** tail;
    uint32_t count = 0;
    if (pn_head) {
        ParseNode* last = pn_head;
        ParseNode* pn = last;
        while (pn) {
            last = pn;
            pn = pn->pn_next;
            count++;
        }

        tail = &last->pn_next;
    } else {
        tail = &pn_head;
    }
    MOZ_ASSERT(pn_tail == tail);
    MOZ_ASSERT(pn_count == count);
}
#endif

/*
 * Allocate a ParseNode from parser's node freelist or, failing that, from
 * cx's temporary arena.
 */
void*
ParseNodeAllocator::allocNode()
{
    LifoAlloc::AutoFallibleScope fallibleAllocator(&alloc);
    void* p = alloc.alloc(sizeof (ParseNode));
    if (!p)
        ReportOutOfMemory(cx);
    return p;
}

ParseNode*
ParseNode::appendOrCreateList(ParseNodeKind kind, ParseNode* left, ParseNode* right,
                              FullParseHandler* handler, ParseContext* pc)
{
    // The asm.js specification is written in ECMAScript grammar terms that
    // specify *only* a binary tree.  It's a royal pain to implement the asm.js
    // spec to act upon n-ary lists as created below.  So for asm.js, form a
    // binary tree of lists exactly as ECMAScript would by skipping the
    // following optimization.
    if (!pc->useAsmOrInsideUseAsm()) {
        // Left-associative trees of a given operator (e.g. |a + b + c|) are
        // binary trees in the spec: (+ (+ a b) c) in Lisp terms.  Recursively
        // processing such a tree, exactly implemented that way, would blow the
        // the stack.  We use a list node that uses O(1) stack to represent
        // such operations: (+ a b c).
        //
        // (**) is right-associative; per spec |a ** b ** c| parses as
        // (** a (** b c)). But we treat this the same way, creating a list
        // node: (** a b c). All consumers must understand that this must be
        // processed with a right fold, whereas the list (+ a b c) must be
        // processed with a left fold because (+) is left-associative.
        //
        if (left->isKind(kind) &&
            (kind == ParseNodeKind::Pow ? !left->pn_parens : left->isBinaryOperation()))
        {
            ListNode* list = &left->as<ListNode>();

            list->append(right);
            list->pn_pos.end = right->pn_pos.end;

            return list;
        }
    }

    ParseNode* list = handler->new_<ListNode>(kind, JSOP_NOP, left);
    if (!list)
        return nullptr;

    list->append(right);
    return list;
}

#ifdef DEBUG

static const char * const parseNodeNames[] = {
#define STRINGIFY(name) #name,
    FOR_EACH_PARSE_NODE_KIND(STRINGIFY)
#undef STRINGIFY
};

void
frontend::DumpParseTree(ParseNode* pn, GenericPrinter& out, int indent)
{
    if (pn == nullptr)
        out.put("#NULL");
    else
        pn->dump(out, indent);
}

static void
IndentNewLine(GenericPrinter& out, int indent)
{
    out.putChar('\n');
    for (int i = 0; i < indent; ++i)
        out.putChar(' ');
}

void
ParseNode::dump(GenericPrinter& out)
{
    dump(out, 0);
    out.putChar('\n');
}

void
ParseNode::dump()
{
    js::Fprinter out(stderr);
    dump(out);
}

void
ParseNode::dump(GenericPrinter& out, int indent)
{
    switch (pn_arity) {
      case PN_NULLARY:
        ((NullaryNode*) this)->dump(out);
        break;
      case PN_UNARY:
        ((UnaryNode*) this)->dump(out, indent);
        break;
      case PN_BINARY:
        ((BinaryNode*) this)->dump(out, indent);
        break;
      case PN_TERNARY:
        ((TernaryNode*) this)->dump(out, indent);
        break;
      case PN_CODE:
        ((CodeNode*) this)->dump(out, indent);
        break;
      case PN_LIST:
        ((ListNode*) this)->dump(out, indent);
        break;
      case PN_NAME:
        ((NameNode*) this)->dump(out, indent);
        break;
      case PN_SCOPE:
        ((LexicalScopeNode*) this)->dump(out, indent);
        break;
      default:
        out.printf("#<BAD NODE %p, kind=%u, arity=%u>",
                (void*) this, unsigned(getKind()), unsigned(pn_arity));
        break;
    }
}

void
NullaryNode::dump(GenericPrinter& out)
{
    switch (getKind()) {
      case ParseNodeKind::True:  out.put("#true");  break;
      case ParseNodeKind::False: out.put("#false"); break;
      case ParseNodeKind::Null:  out.put("#null");  break;
      case ParseNodeKind::RawUndefined: out.put("#undefined"); break;

      case ParseNodeKind::Number: {
        ToCStringBuf cbuf;
        const char* cstr = NumberToCString(nullptr, &cbuf, pn_dval);
        if (!IsFinite(pn_dval))
            out.put("#");
        if (cstr)
            out.printf("%s", cstr);
        else
            out.printf("%g", pn_dval);
        break;
      }

      case ParseNodeKind::String:
      case ParseNodeKind::ObjectPropertyName:
        pn_atom->dumpCharsNoNewline(out);
        break;

      default:
        out.printf("(%s)", parseNodeNames[size_t(getKind())]);
    }
}

void
UnaryNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(pn_kid, out, indent);
    out.printf(")");
}

void
BinaryNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(pn_left, out, indent);
    IndentNewLine(out, indent);
    DumpParseTree(pn_right, out, indent);
    out.printf(")");
}

void
TernaryNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(pn_kid1, out, indent);
    IndentNewLine(out, indent);
    DumpParseTree(pn_kid2, out, indent);
    IndentNewLine(out, indent);
    DumpParseTree(pn_kid3, out, indent);
    out.printf(")");
}

void
CodeNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(pn_body, out, indent);
    out.printf(")");
}

void
ListNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s [", name);
    if (pn_head != nullptr) {
        indent += strlen(name) + 3;
        DumpParseTree(pn_head, out, indent);
        ParseNode* pn = pn_head->pn_next;
        while (pn != nullptr) {
            IndentNewLine(out, indent);
            DumpParseTree(pn, out, indent);
            pn = pn->pn_next;
        }
    }
    out.printf("])");
}

template <typename CharT>
static void
DumpName(GenericPrinter& out, const CharT* s, size_t len)
{
    if (len == 0)
        out.put("#<zero-length name>");

    for (size_t i = 0; i < len; i++) {
        char16_t c = s[i];
        if (c > 32 && c < 127)
            out.putChar(c);
        else if (c <= 255)
            out.printf("\\x%02x", unsigned(c));
        else
            out.printf("\\u%04x", unsigned(c));
    }
}

void
NameNode::dump(GenericPrinter& out, int indent)
{
    if (isKind(ParseNodeKind::Name) || isKind(ParseNodeKind::Dot)) {
        if (isKind(ParseNodeKind::Dot))
            out.put("(.");

        if (!pn_atom) {
            out.put("#<null name>");
        } else if (getOp() == JSOP_GETARG && pn_atom->length() == 0) {
            // Dump destructuring parameter.
            out.put("(#<zero-length name> ");
            DumpParseTree(expr(), out, indent + 21);
            out.printf(")");
        } else {
            JS::AutoCheckCannotGC nogc;
            if (pn_atom->hasLatin1Chars())
                DumpName(out, pn_atom->latin1Chars(nogc), pn_atom->length());
            else
                DumpName(out, pn_atom->twoByteChars(nogc), pn_atom->length());
        }

        if (isKind(ParseNodeKind::Dot)) {
            out.putChar(' ');
            if (as<PropertyAccess>().isSuper())
                out.put("super");
            else
                DumpParseTree(expr(), out, indent + 2);
            out.printf(")");
        }
        return;
    }

    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s ", name);
    indent += strlen(name) + 2;
    DumpParseTree(expr(), out, indent);
    out.printf(")");
}

void
LexicalScopeNode::dump(GenericPrinter& out, int indent)
{
    const char* name = parseNodeNames[size_t(getKind())];
    out.printf("(%s [", name);
    int nameIndent = indent + strlen(name) + 3;
    if (!isEmptyScope()) {
        LexicalScope::Data* bindings = scopeBindings();
        for (uint32_t i = 0; i < bindings->length; i++) {
            JSAtom* name = bindings->trailingNames[i].name();
            JS::AutoCheckCannotGC nogc;
            if (name->hasLatin1Chars())
                DumpName(out, name->latin1Chars(nogc), name->length());
            else
                DumpName(out, name->twoByteChars(nogc), name->length());
            if (i < bindings->length - 1)
                IndentNewLine(out, nameIndent);
        }
    }
    out.putChar(']');
    indent += 2;
    IndentNewLine(out, indent);
    DumpParseTree(scopeBody(), out, indent);
    out.printf(")");
}
#endif

ObjectBox::ObjectBox(JSObject* object, ObjectBox* traceLink)
  : object(object),
    traceLink(traceLink),
    emitLink(nullptr)
{
    MOZ_ASSERT(!object->is<JSFunction>());
    MOZ_ASSERT(object->isTenured());
}

ObjectBox::ObjectBox(JSFunction* function, ObjectBox* traceLink)
  : object(function),
    traceLink(traceLink),
    emitLink(nullptr)
{
    MOZ_ASSERT(object->is<JSFunction>());
    MOZ_ASSERT(asFunctionBox()->function() == function);
    MOZ_ASSERT(object->isTenured());
}

FunctionBox*
ObjectBox::asFunctionBox()
{
    MOZ_ASSERT(isFunctionBox());
    return static_cast<FunctionBox*>(this);
}

/* static */ void
ObjectBox::TraceList(JSTracer* trc, ObjectBox* listHead)
{
    for (ObjectBox* box = listHead; box; box = box->traceLink)
        box->trace(trc);
}

void
ObjectBox::trace(JSTracer* trc)
{
    TraceRoot(trc, &object, "parser.object");
}

void
FunctionBox::trace(JSTracer* trc)
{
    ObjectBox::trace(trc);
    if (enclosingScope_)
        TraceRoot(trc, &enclosingScope_, "funbox-enclosingScope");
}

bool
js::frontend::IsAnonymousFunctionDefinition(ParseNode* pn)
{
    // ES 2017 draft
    // 12.15.2 (ArrowFunction, AsyncArrowFunction).
    // 14.1.12 (FunctionExpression).
    // 14.4.8 (GeneratorExpression).
    // 14.6.8 (AsyncFunctionExpression)
    if (pn->isKind(ParseNodeKind::Function) && !pn->pn_funbox->function()->explicitName())
        return true;

    // 14.5.8 (ClassExpression)
    if (pn->is<ClassNode>() && !pn->as<ClassNode>().names())
        return true;

    return false;
}
