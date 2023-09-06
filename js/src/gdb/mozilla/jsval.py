# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printers for SpiderMonkey's JS::Value.

import struct

import gdb
import gdb.types

import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# See Value.h [SMDOC] JS::Value Boxing Formats for details on the JS::Value boxing
# formats handled below.


class Box(object):
    def __init__(self, asBits, jtc):
        self.asBits = asBits
        self.jtc = jtc
        # Value::asBits is uint64_t, but somehow the sign bit can be botched
        # here, even though Python integers are arbitrary precision.
        if self.asBits < 0:
            self.asBits = self.asBits + (1 << 64)

    # Return this value's type tag.
    def tag(self):
        raise NotImplementedError

    # Return this value as a 32-bit integer, double, or address.
    def as_uint32(self):
        raise NotImplementedError

    def as_double(self):
        packed = struct.pack("q", self.asBits)
        (unpacked,) = struct.unpack("d", packed)
        return unpacked

    def as_address(self):
        raise NotImplementedError


class Punbox(Box):
    # Packed non-number boxing --- the format used on x86_64. It would be nice to
    # simply call Value::toInt32, etc. here, but the debugger is likely to see many
    # Values, and doing several inferior calls for each one seems like a bad idea.

    FULL_WIDTH = 64
    TAG_SHIFT = 47
    PAYLOAD_MASK = (1 << TAG_SHIFT) - 1
    TAG_MASK = (1 << (FULL_WIDTH - TAG_SHIFT)) - 1
    TAG_MAX_DOUBLE = 0x1FFF0
    TAG_TYPE_MASK = 0x0000F

    def tag(self):
        tag = self.asBits >> Punbox.TAG_SHIFT
        if tag <= Punbox.TAG_MAX_DOUBLE:
            return self.jtc.DOUBLE
        else:
            return tag & Punbox.TAG_TYPE_MASK

    def as_uint32(self):
        return int(self.asBits & ((1 << 32) - 1))

    def as_address(self):
        return gdb.Value(self.asBits & Punbox.PAYLOAD_MASK)


class Nunbox(Box):
    TAG_SHIFT = 32
    TAG_CLEAR = 0xFFFF0000
    PAYLOAD_MASK = 0xFFFFFFFF
    TAG_TYPE_MASK = 0x0000000F

    def tag(self):
        tag = self.asBits >> Nunbox.TAG_SHIFT
        if tag < Nunbox.TAG_CLEAR:
            return self.jtc.DOUBLE
        return tag & Nunbox.TAG_TYPE_MASK

    def as_uint32(self):
        return int(self.asBits & Nunbox.PAYLOAD_MASK)

    def as_address(self):
        return gdb.Value(self.asBits & Nunbox.PAYLOAD_MASK)


class JSValueTypeCache(object):
    # Cache information about the Value type for this objfile.

    def __init__(self, cache):
        # Capture the tag values.
        d = gdb.types.make_enum_dict(gdb.lookup_type("JSValueType"))

        # The enum keys are prefixed when building with some compilers (clang at
        # a minimum), so use a helper function to handle either key format.
        def get(key):
            val = d.get(key)
            if val is not None:
                return val
            return d["JSValueType::" + key]

        self.DOUBLE = get("JSVAL_TYPE_DOUBLE")
        self.INT32 = get("JSVAL_TYPE_INT32")
        self.UNDEFINED = get("JSVAL_TYPE_UNDEFINED")
        self.BOOLEAN = get("JSVAL_TYPE_BOOLEAN")
        self.MAGIC = get("JSVAL_TYPE_MAGIC")
        self.STRING = get("JSVAL_TYPE_STRING")
        self.SYMBOL = get("JSVAL_TYPE_SYMBOL")
        self.BIGINT = get("JSVAL_TYPE_BIGINT")
        self.NULL = get("JSVAL_TYPE_NULL")
        self.OBJECT = get("JSVAL_TYPE_OBJECT")

        # Let self.magic_names be an array whose i'th element is the name of
        # the i'th magic value.
        d = gdb.types.make_enum_dict(gdb.lookup_type("JSWhyMagic"))
        self.magic_names = list(range(max(d.values()) + 1))
        for k, v in d.items():
            self.magic_names[v] = k

        # Choose an unboxing scheme for this architecture.
        self.boxer = Punbox if cache.void_ptr_t.sizeof == 8 else Nunbox


@pretty_printer("JS::Value")
class JSValue(object):
    def __init__(self, value, cache):
        # Save the generic typecache, and create our own, if we haven't already.
        self.cache = cache
        if not cache.mod_JS_Value:
            cache.mod_JS_Value = JSValueTypeCache(cache)
        self.jtc = cache.mod_JS_Value

        self.value = value
        self.box = self.jtc.boxer(value["asBits_"], self.jtc)

    def to_string(self):
        tag = self.box.tag()

        if tag == self.jtc.UNDEFINED:
            return "$JS::UndefinedValue()"
        if tag == self.jtc.NULL:
            return "$JS::NullValue()"
        if tag == self.jtc.BOOLEAN:
            return "$JS::BooleanValue(%s)" % str(self.box.as_uint32() != 0).lower()
        if tag == self.jtc.MAGIC:
            value = self.box.as_uint32()
            if 0 <= value and value < len(self.jtc.magic_names):
                return "$JS::MagicValue(%s)" % (self.jtc.magic_names[value],)
            else:
                return "$JS::MagicValue(%d)" % (value,)

        if tag == self.jtc.INT32:
            value = self.box.as_uint32()
            signbit = 1 << 31
            value = (value ^ signbit) - signbit
            return "$JS::Int32Value(%s)" % value

        if tag == self.jtc.DOUBLE:
            return "$JS::DoubleValue(%s)" % self.box.as_double()

        if tag == self.jtc.STRING:
            value = self.box.as_address().cast(self.cache.JSString_ptr_t)
        elif tag == self.jtc.OBJECT:
            value = self.box.as_address().cast(self.cache.JSObject_ptr_t)
        elif tag == self.jtc.SYMBOL:
            value = self.box.as_address().cast(self.cache.JSSymbol_ptr_t)
        elif tag == self.jtc.BIGINT:
            return "$JS::BigIntValue()"
        else:
            value = "unrecognized!"
        return "$JS::Value(%s)" % (value,)

    def is_undefined(self):
        return self.box.tag() == self.jtc.UNDEFINED

    def get_string(self):
        assert self.box.tag() == self.jtc.STRING
        return self.box.as_address().cast(self.cache.JSString_ptr_t)

    def get_private(self):
        assert self.box.tag() == self.jtc.DOUBLE
        return self.box.asBits
