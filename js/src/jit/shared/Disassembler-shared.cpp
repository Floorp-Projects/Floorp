/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/Disassembler-shared.h"

#include "jsprf.h"

#include "jit/JitSpewer.h"

using namespace js::jit;

#ifdef JS_DISASM_SUPPORTED
// See comments in spew(), below.
mozilla::Atomic<uint32_t> DisassemblerSpew::live_(0);
#endif

DisassemblerSpew::DisassemblerSpew()
  : printer_(nullptr)
#ifdef JS_DISASM_SUPPORTED
    ,
    labelIndent_(""),
    targetIndent_(""),
    spewNext_(1000),
    nodes_(nullptr)
#endif
{
#ifdef JS_DISASM_SUPPORTED
    live_++;
#endif
}

DisassemblerSpew::~DisassemblerSpew()
{
#ifdef JS_DISASM_SUPPORTED
    Node* p = nodes_;
    while (p) {
        Node* victim = p;
        p = p->next;
        js_free(victim);
    }
    live_--;
#endif
}

void
DisassemblerSpew::setPrinter(Sprinter* printer)
{
    printer_ = printer;
}

bool
DisassemblerSpew::isDisabled()
{
    return !(JitSpewEnabled(JitSpew_Codegen) || printer_);
}

void
DisassemblerSpew::spew(const char* fmt, ...)
{
    // Nested assemblers are handled by prefixing the output with '>..> ' where
    // the number of '>' is the nesting level, and the outermost assembler is
    // taken as being at nesting level zero (and does not require the trailing
    // space character).  This markup disambiguates eg the output of an IC
    // compilation that happens as a subtask of a normal compilation from the
    // output of the normal compilation.
    //
    // We track the nesting level globally, on the assumption that anyone
    // wanting to look at disassembly is running with --no-threads.  If this
    // turns out to be wrong then live_ can be made thread-local.

#ifdef JS_DISASM_SUPPORTED
    char fmt2[1024];
    MOZ_RELEASE_ASSERT(sizeof(fmt2) >= strlen(fmt) + live_ + 1);
    uint32_t i;
    for (i = 0; i < live_-1; i++ )
        fmt2[i] = '>';
    if (live_ > 1)
        fmt2[i++] = ' ';
    strcpy(fmt2 + i, fmt);
#else
    const char* fmt2 = fmt;
#endif

    va_list args;
    va_start(args, fmt);
    spewVA(fmt2, args);
    va_end(args);
}

void
DisassemblerSpew::spewVA(const char* fmt, va_list va)
{
    if (printer_) {
        printer_->vprintf(fmt, va);
        printer_->put("\n");
    }
    js::jit::JitSpewVA(js::jit::JitSpew_Codegen, fmt, va);
}

#ifdef JS_DISASM_SUPPORTED

void
DisassemblerSpew::setLabelIndent(const char* s)
{
    labelIndent_ = s;
}

void
DisassemblerSpew::setTargetIndent(const char* s)
{
    targetIndent_ = s;
}

DisassemblerSpew::LabelDoc
DisassemblerSpew::refLabel(const Label* l)
{
    return l ? LabelDoc(internalResolve(l), l->bound()) : LabelDoc();
}

void
DisassemblerSpew::spewRef(const LabelDoc& target)
{
    if (isDisabled())
        return;
    if (!target.valid)
        return;
    spew("%s-> %d%s", targetIndent_, target.doc, !target.bound ? "f" : "");
}

void
DisassemblerSpew::spewBind(const Label* label)
{
    if (isDisabled())
        return;
    uint32_t v = internalResolve(label);
    Node* probe = lookup(label);
    if (probe)
        probe->bound = true;
    spew("%s%d:", labelIndent_, v);
}

void
DisassemblerSpew::spewRetarget(const Label* label, const Label* target)
{
    if (isDisabled())
        return;
    LabelDoc labelDoc = LabelDoc(internalResolve(label), label->bound());
    LabelDoc targetDoc = LabelDoc(internalResolve(target), target->bound());
    Node* probe = lookup(label);
    if (probe)
        probe->bound = true;
    spew("%s%d: .retarget -> %d%s",
         labelIndent_, labelDoc.doc, targetDoc.doc, !targetDoc.bound ? "f" : "");
}

void
DisassemblerSpew::formatLiteral(const LiteralDoc& doc, char* buffer, size_t bufsize)
{
    switch (doc.type) {
      case LiteralDoc::Type::Patchable:
        snprintf(buffer, bufsize, "patchable");
        break;
      case LiteralDoc::Type::I32:
        snprintf(buffer, bufsize, "%d", doc.value.i32);
        break;
      case LiteralDoc::Type::U32:
        snprintf(buffer, bufsize, "%u", doc.value.u32);
        break;
      case LiteralDoc::Type::I64:
        snprintf(buffer, bufsize, "%" PRIi64, doc.value.i64);
        break;
      case LiteralDoc::Type::U64:
        snprintf(buffer, bufsize, "%" PRIu64, doc.value.u64);
        break;
      case LiteralDoc::Type::F32:
        snprintf(buffer, bufsize, "%g", doc.value.f32);
        break;
      case LiteralDoc::Type::F64:
        snprintf(buffer, bufsize, "%g", doc.value.f64);
        break;
      default:
        MOZ_CRASH();
    }
}

void
DisassemblerSpew::spewOrphans()
{
    for (Node* p = nodes_; p; p = p->next) {
        if (!p->bound)
            spew("%s%d:    ; .orphan", labelIndent_, p->value);
    }
}

uint32_t
DisassemblerSpew::internalResolve(const Label* l)
{
    // Note, internalResolve will sometimes return 0 when it is triggered by the
    // profiler and not by a full disassembly, since in that case a label can be
    // used or bound but not previously have been defined.  In that case,
    // internalResolve(l) will not necessarily create a binding for l!
    // Consequently a subsequent lookup(l) may still return null.
    return l->used() || l->bound() ? probe(l) : define(l);
}

uint32_t
DisassemblerSpew::probe(const Label* l)
{
    Node* n = lookup(l);
    return n ? n->value : 0;
}

uint32_t
DisassemblerSpew::define(const Label* l)
{
    remove(l);
    uint32_t value = spewNext_++;
    if (!add(l, value))
        return 0;
    return value;
}

DisassemblerSpew::Node*
DisassemblerSpew::lookup(const Label* key)
{
    Node* p;
    for (p = nodes_; p && p->key != key; p = p->next)
        ;
    return p;
}

DisassemblerSpew::Node*
DisassemblerSpew::add(const Label* key, uint32_t value)
{
    MOZ_ASSERT(!lookup(key));
    Node* node = (Node*)js_malloc(sizeof(Node));
    if (node) {
        node->key = key;
        node->value = value;
        node->bound = false;
        node->next = nodes_;
        nodes_ = node;
    }
    return node;
}

bool
DisassemblerSpew::remove(const Label* key)
{
    // We do not require that there is a node matching the key.
    for (Node* p = nodes_, *pp = nullptr; p; pp = p, p = p->next) {
        if (p->key == key) {
            if (pp)
                pp->next = p->next;
            else
                nodes_ = p->next;
            js_free(p);
            return true;
        }
    }
    return false;
}

#else

DisassemblerSpew::LabelDoc
DisassemblerSpew::refLabel(const Label* l)
{
    return LabelDoc();
}

#endif
