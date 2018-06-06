# Pretty-printers for SpiderMonkey's JS::Value.

import gdb
import gdb.types
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Summary of the JS::Value type:
#
# JS::Value is a 64-bit discriminated union, with JSString*, JSObject*, IEEE
# 64-bit floating-point, and 32-bit integer branches (and a few others).
# JS::Value is 64 bits long on all architectures.
#
# The ECMAScript standard specifies that ECMAScript numbers are IEEE 64-bit
# floating-point values. A JS::Value can represent any JavaScript number
# value directly, without referring to additional storage, or represent an
# object, string, or other ECMAScript value, and remember which type it is.
# This may seem surprising: how can a 64-bit type hold all the 64-bit IEEE
# values, and still distinguish them from objects, strings, and so on,
# which have 64-bit addresses?
#
# This is possible for two reasons:
#
# - First, ECMAScript implementations aren't required to distinguish all
#   the values the IEEE 64-bit format can represent. The IEEE format
#   specifies many bitstrings representing NaN values, while ECMAScript
#   requires only a single NaN value. This means we can use one IEEE NaN to
#   represent ECMAScript's NaN, and use all the other IEEE NaNs to
#   represent the other ECMAScript values.
#
#   (IEEE says that any floating-point value whose 11-bit exponent field is
#   0x7ff (all ones) and whose 52-bit fraction field is non-zero is a NaN.
#   So as long as we ensure the fraction field is non-zero, and save a NaN
#   for ECMAScript, we have 2^52 values to play with.)
#
# - Second, on the only 64-bit architecture we support, x86_64, only the
#   lower 48 bits of an address are significant. The upper sixteen bits are
#   required to be the sign-extension of bit 48. Furthermore, user code
#   always runs in "positive addresses": those in which bit 48 is zero. So
#   we only actually need 47 bits to store all possible object or string
#   addresses, even on 64-bit platforms.
#
# With a 52-bit fraction field, and 47 bits needed for the 'payload', we
# have up to five bits left to store a 'tag' value, to indicate which
# branch of our discriminated union is live.
#
# Thus, we define JS::Value representations in terms of the IEEE 64-bit
# floating-point format:
#
# - Any bitstring that IEEE calls a number or an infinity represents that
#   ECMAScript number.
#
# - Any bitstring that IEEE calls a NaN represents either an ECMAScript NaN
#   or a non-number ECMAScript value, as determined by a tag field stored
#   towards the most significant end of the fraction field (exactly where
#   depends on the address size). If the tag field indicates that this
#   JS::Value is an object, the fraction field's least significant end
#   holds the address of a JSObject; if a string, the address of a
#   JSString; and so on.
#
# On x86_64 only the lower 48 bits of an address are significant, and only
# those values whose top bit is zero are used for user-space addresses. Thus
# x86_64 addresses are effectively 47 bits long and fit nicely in the available
# portion of the fraction field.
#
# See Value.h for full details.


class Box(object):
    def __init__(self, asBits, jtc):
        self.asBits = asBits
        self.jtc = jtc
        # Value::asBits is uint64_t, but somehow the sign bit can be botched
        # here, even though Python integers are arbitrary precision.
        if self.asBits < 0:
            self.asBits = self.asBits + (1 << 64)

    # Return this value's type tag.
    def tag(self): raise NotImplementedError

    # Return this value as a 32-bit integer, double, or address.
    def as_uint32(self): raise NotImplementedError

    def as_double(self): raise NotImplementedError

    def as_address(self): raise NotImplementedError


class Punbox(Box):
    # Packed non-number boxing --- the format used on x86_64. It would be nice to
    # simply call Value::toInt32, etc. here, but the debugger is likely to see many
    # Values, and doing several inferior calls for each one seems like a bad idea.

    FULL_WIDTH = 64
    TAG_SHIFT = 47
    PAYLOAD_MASK = (1 << TAG_SHIFT) - 1
    TAG_MASK = (1 << (FULL_WIDTH - TAG_SHIFT)) - 1
    TAG_MAX_DOUBLE = 0x1fff0
    TAG_TYPE_MASK = 0x0000f

    def tag(self):
        tag = self.asBits >> Punbox.TAG_SHIFT
        if tag <= Punbox.TAG_MAX_DOUBLE:
            return self.jtc.DOUBLE
        else:
            return tag & Punbox.TAG_TYPE_MASK

    def as_uint32(self): return int(self.asBits & ((1 << 32) - 1))

    def as_address(self): return gdb.Value(self.asBits & Punbox.PAYLOAD_MASK)


class Nunbox(Box):
    TAG_SHIFT = 32
    TAG_CLEAR = 0xffff0000
    PAYLOAD_MASK = 0xffffffff
    TAG_TYPE_MASK = 0x0000000f

    def tag(self):
        tag = self.asBits >> Nunbox.TAG_SHIFT
        if tag < Nunbox.TAG_CLEAR:
            return self.jtc.DOUBLE
        return tag & Nunbox.TAG_TYPE_MASK

    def as_uint32(self): return int(self.asBits & Nunbox.PAYLOAD_MASK)

    def as_address(self): return gdb.Value(self.asBits & Nunbox.PAYLOAD_MASK)


class JSValueTypeCache(object):
    # Cache information about the Value type for this objfile.

    def __init__(self, cache):
        # Capture the tag values.
        d = gdb.types.make_enum_dict(gdb.lookup_type('JSValueType'))

        # The enum keys are prefixed when building with some compilers (clang at
        # a minimum), so use a helper function to handle either key format.
        def get(key):
            val = d.get(key)
            if val is not None:
                return val
            return d['JSValueType::' + key]

        self.DOUBLE = get('JSVAL_TYPE_DOUBLE')
        self.INT32 = get('JSVAL_TYPE_INT32')
        self.UNDEFINED = get('JSVAL_TYPE_UNDEFINED')
        self.BOOLEAN = get('JSVAL_TYPE_BOOLEAN')
        self.MAGIC = get('JSVAL_TYPE_MAGIC')
        self.STRING = get('JSVAL_TYPE_STRING')
        self.SYMBOL = get('JSVAL_TYPE_SYMBOL')
        self.NULL = get('JSVAL_TYPE_NULL')
        self.OBJECT = get('JSVAL_TYPE_OBJECT')

        self.enable_bigint = False
        try:
            # Looking up the tag will throw an exception if BigInt is not
            # enabled.
            self.BIGINT = get('JSVAL_TYPE_BIGINT')
            self.enable_bigint = True
        except Exception:
            pass

        # Let self.magic_names be an array whose i'th element is the name of
        # the i'th magic value.
        d = gdb.types.make_enum_dict(gdb.lookup_type('JSWhyMagic'))
        self.magic_names = list(range(max(d.values()) + 1))
        for (k, v) in d.items():
            self.magic_names[v] = k

        # Choose an unboxing scheme for this architecture.
        self.boxer = Punbox if cache.void_ptr_t.sizeof == 8 else Nunbox


@pretty_printer('JS::Value')
class JSValue(object):
    def __init__(self, value, cache):
        # Save the generic typecache, and create our own, if we haven't already.
        self.cache = cache
        if not cache.mod_JS_Value:
            cache.mod_JS_Value = JSValueTypeCache(cache)
        self.jtc = cache.mod_JS_Value

        self.value = value
        self.box = self.jtc.boxer(value['asBits_'], self.jtc)

    def to_string(self):
        tag = self.box.tag()

        if tag == self.jtc.UNDEFINED:
            return '$JS::UndefinedValue()'
        if tag == self.jtc.NULL:
            return '$JS::NullValue()'
        if tag == self.jtc.BOOLEAN:
            return '$JS::BooleanValue(%s)' % str(self.box.as_uint32() != 0).lower()
        if tag == self.jtc.MAGIC:
            value = self.box.as_uint32()
            if 0 <= value and value < len(self.jtc.magic_names):
                return '$JS::MagicValue(%s)' % (self.jtc.magic_names[value],)
            else:
                return '$JS::MagicValue(%d)' % (value,)

        if tag == self.jtc.INT32:
            value = self.box.as_uint32()
            signbit = 1 << 31
            value = (value ^ signbit) - signbit
            return '$JS::Int32Value(%s)' % value

        if tag == self.jtc.DOUBLE:
            return '$JS::DoubleValue(%s)' % self.value['asDouble_']

        if tag == self.jtc.STRING:
            value = self.box.as_address().cast(self.cache.JSString_ptr_t)
        elif tag == self.jtc.OBJECT:
            value = self.box.as_address().cast(self.cache.JSObject_ptr_t)
        elif tag == self.jtc.SYMBOL:
            value = self.box.as_address().cast(self.cache.JSSymbol_ptr_t)
        elif self.jtc.enable_bigint and tag == self.jtc.BIGINT:
            return '$JS::BigIntValue()'
        else:
            value = 'unrecognized!'
        return '$JS::Value(%s)' % (value,)
