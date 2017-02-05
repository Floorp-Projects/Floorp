/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JSONPrinter_h
#define jit_JSONPrinter_h

#ifdef JS_JITSPEW

#include <stdio.h>

#include "js/TypeDecls.h"
#include "vm/Printer.h"

namespace js {
namespace jit {

class JSONPrinter
{
  protected:
    int indentLevel_;
    bool first_;
    GenericPrinter& out_;

    void indent();

    void property(const char* name);
    void beginObject();
    void beginObjectProperty(const char* name);
    void beginListProperty(const char* name);
    void stringValue(const char* format, ...) MOZ_FORMAT_PRINTF(2, 3);
    void stringProperty(const char* name, const char* format, ...) MOZ_FORMAT_PRINTF(3, 4);
    void beginStringProperty(const char* name);
    void endStringProperty();
    void integerValue(int value);
    void integerProperty(const char* name, int value);
    void endObject();
    void endList();

  public:
    explicit JSONPrinter(GenericPrinter& out)
      : indentLevel_(0),
        first_(true),
        out_(out)
    { }
};

} // namespace jit
} // namespace js

#endif /* JS_JITSPEW */

#endif /* jit_JSONPrinter_h */
