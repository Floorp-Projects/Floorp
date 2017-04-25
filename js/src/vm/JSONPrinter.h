/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSONPrinter_h
#define vm_JSONPrinter_h

#include <stdio.h>

#include "js/TypeDecls.h"
#include "vm/Printer.h"

namespace js {

class JSONPrinter
{
  protected:
    int indentLevel_;
    bool first_;
    GenericPrinter& out_;

    void indent();

  public:
    explicit JSONPrinter(GenericPrinter& out)
      : indentLevel_(0),
        first_(true),
        out_(out)
    { }

    void beginObject();
    void beginObjectProperty(const char* name);
    void beginListProperty(const char* name);

    void value(const char* format, ...) MOZ_FORMAT_PRINTF(2, 3);
    void value(int value);

    void property(const char* name, const char* format, ...) MOZ_FORMAT_PRINTF(3, 4);
    void property(const char* name, int value);
    void property(const char* name, uint32_t value);
    void property(const char* name, uint64_t value);
    void property(const char* name, double value);

    void beginStringProperty(const char* name);
    void endStringProperty();

    void endObject();
    void endList();

  protected:
    void propertyName(const char* name);
};

} // namespace js

#endif /* vm_JSONPrinter_h */
