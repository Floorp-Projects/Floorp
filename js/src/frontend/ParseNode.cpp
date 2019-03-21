/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNode.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/FloatingPoint.h"

#include "jsnum.h"

#include "frontend/Parser.h"

#include "vm/JSContext-inl.h"

using namespace js;
using namespace js::frontend;

using mozilla::ArrayLength;
using mozilla::IsFinite;

#ifdef DEBUG
void ListNode::checkConsistency() const {
  ParseNode* const* tailNode;
  uint32_t actualCount = 0;
  if (const ParseNode* last = head()) {
    const ParseNode* pn = last;
    while (pn) {
      last = pn;
      pn = pn->pn_next;
      actualCount++;
    }

    tailNode = &last->pn_next;
  } else {
    tailNode = &head_;
  }
  MOZ_ASSERT(tail() == tailNode);
  MOZ_ASSERT(count() == actualCount);
}
#endif

/*
 * Allocate a ParseNode from parser's node freelist or, failing that, from
 * cx's temporary arena.
 */
void* ParseNodeAllocator::allocNode(size_t size) {
  LifoAlloc::AutoFallibleScope fallibleAllocator(&alloc);
  void* p = alloc.alloc(size);
  if (!p) {
    ReportOutOfMemory(cx);
  }
  return p;
}

ParseNode* ParseNode::appendOrCreateList(ParseNodeKind kind, ParseNode* left,
                                         ParseNode* right,
                                         FullParseHandler* handler,
                                         ParseContext* pc) {
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
        (kind == ParseNodeKind::PowExpr ? !left->isInParens()
                                        : left->isBinaryOperation())) {
      ListNode* list = &left->as<ListNode>();

      list->append(right);
      list->pn_pos.end = right->pn_pos.end;

      return list;
    }
  }

  ListNode* list = handler->new_<ListNode>(kind, left);
  if (!list) {
    return nullptr;
  }

  list->append(right);
  return list;
}

const ParseNode::TypeCode ParseNode::typeCodeTable[] = {
#define TYPE_CODE(_name, type) type::classTypeCode(),
    FOR_EACH_PARSE_NODE_KIND(TYPE_CODE)
#undef TYPE_CODE
};

#ifdef DEBUG

static const char* const parseNodeNames[] = {
#  define STRINGIFY(name, _type) #  name,
    FOR_EACH_PARSE_NODE_KIND(STRINGIFY)
#  undef STRINGIFY
};

void frontend::DumpParseTree(ParseNode* pn, GenericPrinter& out, int indent) {
  if (pn == nullptr) {
    out.put("#NULL");
  } else {
    pn->dump(out, indent);
  }
}

static void IndentNewLine(GenericPrinter& out, int indent) {
  out.putChar('\n');
  for (int i = 0; i < indent; ++i) {
    out.putChar(' ');
  }
}

void ParseNode::dump(GenericPrinter& out) {
  dump(out, 0);
  out.putChar('\n');
}

void ParseNode::dump() {
  js::Fprinter out(stderr);
  dump(out);
}

void ParseNode::dump(GenericPrinter& out, int indent) {
  switch (getKind()) {
#  define DUMP(K, T)                 \
    case ParseNodeKind::K:           \
      as<T>().dumpImpl(out, indent); \
      break;
    FOR_EACH_PARSE_NODE_KIND(DUMP)
#  undef DUMP
    default:
      out.printf("#<BAD NODE %p, kind=%u>", (void*)this, unsigned(getKind()));
  }
}

void NullaryNode::dumpImpl(GenericPrinter& out, int indent) {
  switch (getKind()) {
    case ParseNodeKind::TrueExpr:
      out.put("#true");
      break;
    case ParseNodeKind::FalseExpr:
      out.put("#false");
      break;
    case ParseNodeKind::NullExpr:
      out.put("#null");
      break;
    case ParseNodeKind::RawUndefinedExpr:
      out.put("#undefined");
      break;

    default:
      out.printf("(%s)", parseNodeNames[size_t(getKind())]);
  }
}

void NumericLiteral::dumpImpl(GenericPrinter& out, int indent) {
  ToCStringBuf cbuf;
  const char* cstr = NumberToCString(nullptr, &cbuf, value());
  if (!IsFinite(value())) {
    out.put("#");
  }
  if (cstr) {
    out.printf("%s", cstr);
  } else {
    out.printf("%g", value());
  }
}

void BigIntLiteral::dumpImpl(GenericPrinter& out, int indent) {
  out.printf("(%s)", parseNodeNames[size_t(getKind())]);
}

void RegExpLiteral::dumpImpl(GenericPrinter& out, int indent) {
  out.printf("(%s)", parseNodeNames[size_t(getKind())]);
}

void LoopControlStatement::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s", name);
  if (label()) {
    out.printf(" ");
    label()->dumpCharsNoNewline(out);
  }
  out.printf(")");
}

void UnaryNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  indent += strlen(name) + 2;
  DumpParseTree(kid(), out, indent);
  out.printf(")");
}

void BinaryNode::dumpImpl(GenericPrinter& out, int indent) {
  if (isKind(ParseNodeKind::DotExpr)) {
    out.put("(.");

    DumpParseTree(right(), out, indent + 2);

    out.putChar(' ');
    if (as<PropertyAccess>().isSuper()) {
      out.put("super");
    } else {
      DumpParseTree(left(), out, indent + 2);
    }

    out.printf(")");
    return;
  }

  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  indent += strlen(name) + 2;
  DumpParseTree(left(), out, indent);
  IndentNewLine(out, indent);
  DumpParseTree(right(), out, indent);
  out.printf(")");
}

void TernaryNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  indent += strlen(name) + 2;
  DumpParseTree(kid1(), out, indent);
  IndentNewLine(out, indent);
  DumpParseTree(kid2(), out, indent);
  IndentNewLine(out, indent);
  DumpParseTree(kid3(), out, indent);
  out.printf(")");
}

void FunctionNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  indent += strlen(name) + 2;
  DumpParseTree(body(), out, indent);
  out.printf(")");
}

void ModuleNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  indent += strlen(name) + 2;
  DumpParseTree(body(), out, indent);
  out.printf(")");
}

void ListNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s [", name);
  if (ParseNode* listHead = head()) {
    indent += strlen(name) + 3;
    DumpParseTree(listHead, out, indent);
    for (ParseNode* item : contentsFrom(listHead->pn_next)) {
      IndentNewLine(out, indent);
      DumpParseTree(item, out, indent);
    }
  }
  out.printf("])");
}

template <typename CharT>
static void DumpName(GenericPrinter& out, const CharT* s, size_t len) {
  if (len == 0) {
    out.put("#<zero-length name>");
  }

  for (size_t i = 0; i < len; i++) {
    char16_t c = s[i];
    if (c > 32 && c < 127) {
      out.putChar(c);
    } else if (c <= 255) {
      out.printf("\\x%02x", unsigned(c));
    } else {
      out.printf("\\u%04x", unsigned(c));
    }
  }
}

void NameNode::dumpImpl(GenericPrinter& out, int indent) {
  switch (getKind()) {
    case ParseNodeKind::StringExpr:
    case ParseNodeKind::TemplateStringExpr:
    case ParseNodeKind::ObjectPropertyName:
      atom()->dumpCharsNoNewline(out);
      return;

    case ParseNodeKind::Name:
    case ParseNodeKind::PrivateName:  // atom() already includes the '#', no
                                      // need to specially include it.
    case ParseNodeKind::PropertyNameExpr:
      if (!atom()) {
        out.put("#<null name>");
      } else {
        JS::AutoCheckCannotGC nogc;
        if (atom()->hasLatin1Chars()) {
          DumpName(out, atom()->latin1Chars(nogc), atom()->length());
        } else {
          DumpName(out, atom()->twoByteChars(nogc), atom()->length());
        }
      }
      return;

    case ParseNodeKind::LabelStmt: {
      this->as<LabeledStatement>().dumpImpl(out, indent);
      return;
    }

    default: {
      const char* name = parseNodeNames[size_t(getKind())];
      out.printf("(%s)", name);
      return;
    }
  }
}

void LabeledStatement::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s ", name);
  atom()->dumpCharsNoNewline(out);
  out.printf(" ");
  indent += strlen(name) + atom()->length() + 3;
  DumpParseTree(statement(), out, indent);
  out.printf(")");
}

void LexicalScopeNode::dumpImpl(GenericPrinter& out, int indent) {
  const char* name = parseNodeNames[size_t(getKind())];
  out.printf("(%s [", name);
  int nameIndent = indent + strlen(name) + 3;
  if (!isEmptyScope()) {
    LexicalScope::Data* bindings = scopeBindings();
    for (uint32_t i = 0; i < bindings->length; i++) {
      JSAtom* name = bindings->trailingNames[i].name();
      JS::AutoCheckCannotGC nogc;
      if (name->hasLatin1Chars()) {
        DumpName(out, name->latin1Chars(nogc), name->length());
      } else {
        DumpName(out, name->twoByteChars(nogc), name->length());
      }
      if (i < bindings->length - 1) {
        IndentNewLine(out, nameIndent);
      }
    }
  }
  out.putChar(']');
  indent += 2;
  IndentNewLine(out, indent);
  DumpParseTree(scopeBody(), out, indent);
  out.printf(")");
}
#endif

TraceListNode::TraceListNode(js::gc::Cell* gcThing, TraceListNode* traceLink)
    : gcThing(gcThing), traceLink(traceLink) {
  MOZ_ASSERT(gcThing->isTenured());
}

BigIntBox* TraceListNode::asBigIntBox() {
  MOZ_ASSERT(isBigIntBox());
  return static_cast<BigIntBox*>(this);
}

ObjectBox* TraceListNode::asObjectBox() {
  MOZ_ASSERT(isObjectBox());
  return static_cast<ObjectBox*>(this);
}

BigIntBox::BigIntBox(BigInt* bi, TraceListNode* traceLink)
    : TraceListNode(bi, traceLink) {}

ObjectBox::ObjectBox(JSObject* obj, TraceListNode* traceLink)
    : TraceListNode(obj, traceLink), emitLink(nullptr) {
  MOZ_ASSERT(!object()->is<JSFunction>());
}

ObjectBox::ObjectBox(JSFunction* function, TraceListNode* traceLink)
    : TraceListNode(function, traceLink), emitLink(nullptr) {
  MOZ_ASSERT(object()->is<JSFunction>());
  MOZ_ASSERT(asFunctionBox()->function() == function);
}

FunctionBox* ObjectBox::asFunctionBox() {
  MOZ_ASSERT(isFunctionBox());
  return static_cast<FunctionBox*>(this);
}

/* static */
void TraceListNode::TraceList(JSTracer* trc, TraceListNode* listHead) {
  for (TraceListNode* node = listHead; node; node = node->traceLink) {
    node->trace(trc);
  }
}

void TraceListNode::trace(JSTracer* trc) {
  TraceGenericPointerRoot(trc, &gcThing, "parser.traceListNode");
}

void FunctionBox::trace(JSTracer* trc) {
  ObjectBox::trace(trc);
  if (enclosingScope_) {
    TraceRoot(trc, &enclosingScope_, "funbox-enclosingScope");
  }
}

bool js::frontend::IsAnonymousFunctionDefinition(ParseNode* pn) {
  // ES 2017 draft
  // 12.15.2 (ArrowFunction, AsyncArrowFunction).
  // 14.1.12 (FunctionExpression).
  // 14.4.8 (Generatoression).
  // 14.6.8 (AsyncFunctionExpression)
  if (pn->is<FunctionNode>() &&
      !pn->as<FunctionNode>().funbox()->function()->explicitName()) {
    return true;
  }

  // 14.5.8 (ClassExpression)
  if (pn->is<ClassNode>() && !pn->as<ClassNode>().names()) {
    return true;
  }

  return false;
}
