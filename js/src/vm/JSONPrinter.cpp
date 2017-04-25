/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/JSONPrinter.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/SizePrintfMacros.h"

#include <stdarg.h>

using namespace js;

void
JSONPrinter::indent()
{
    MOZ_ASSERT(indentLevel_ >= 0);
    out_.printf("\n");
    for (int i = 0; i < indentLevel_; i++)
        out_.printf("  ");
}

void
JSONPrinter::propertyName(const char* name)
{
    if (!first_)
        out_.printf(",");
    indent();
    out_.printf("\"%s\":", name);
    first_ = false;
}

void
JSONPrinter::beginObject()
{
    if (!first_) {
        out_.printf(",");
        indent();
    }
    out_.printf("{");
    indentLevel_++;
    first_ = true;
}

void
JSONPrinter::beginObjectProperty(const char* name)
{
    propertyName(name);
    out_.printf("{");
    indentLevel_++;
    first_ = true;
}

void
JSONPrinter::beginListProperty(const char* name)
{
    propertyName(name);
    out_.printf("[");
    first_ = true;
}

void
JSONPrinter::beginStringProperty(const char* name)
{
    propertyName(name);
    out_.printf("\"");
}

void
JSONPrinter::endStringProperty()
{
    out_.printf("\"");
}

void
JSONPrinter::property(const char* name, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);

    beginStringProperty(name);
    out_.vprintf(format, ap);
    endStringProperty();

    va_end(ap);
}

void
JSONPrinter::value(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);

    if (!first_)
        out_.printf(",");
    out_.printf("\"");
    out_.vprintf(format, ap);
    out_.printf("\"");

    va_end(ap);
    first_ = false;
}

void
JSONPrinter::property(const char* name, int value)
{
    propertyName(name);
    out_.printf("%d", value);
}

void
JSONPrinter::value(int val)
{
    if (!first_)
        out_.printf(",");
    out_.printf("%d", val);
    first_ = false;
}

void
JSONPrinter::property(const char* name, uint32_t value)
{
    propertyName(name);
    out_.printf("%u", value);
}

void
JSONPrinter::property(const char* name, uint64_t value)
{
    propertyName(name);
    out_.printf("%" PRIu64, value);
}

void
JSONPrinter::property(const char* name, double value)
{
    propertyName(name);
    if (mozilla::IsFinite(value))
        out_.printf("%f", value);
    else
        out_.printf("null");
}

void
JSONPrinter::endObject()
{
    indentLevel_--;
    indent();
    out_.printf("}");
    first_ = false;
}

void
JSONPrinter::endList()
{
    out_.printf("]");
    first_ = false;
}
