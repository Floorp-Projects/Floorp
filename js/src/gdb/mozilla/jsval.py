# Pretty-printers for SpiderMonkey jsvals.

import gdb
import gdb.types
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, ptr_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Summary of the JS::Value (also known as jsval) type:
#
# Viewed abstractly, JS::Value is a 64-bit discriminated union, with
# JSString *, JSObject *, IEEE 64-bit floating-point, and 32-bit integer
# branches (and a few others). (It is not actually a C++ union;
# 'discriminated union' just describes the overall effect.) Note that
# JS::Value is always 64 bits long, even on 32-bit architectures.
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
# On the only 64-bit platform we support, x86_64, only the lower 48 bits of
# an address are significant, and only those values whose top bit is zero
# are used for user-space addresses. This means that x86_64 addresses are
# effectively 47 bits long, and thus fit nicely in the available portion of
# the fraction field.
#
#
# In detail:
#
# - jsval (Value.h) is a typedef for JS::Value.
#
# - JS::Value (Value.h) is a class with a lot of methods and a single data
#   member, of type jsval_layout.
#
# - jsval_layout (Value.h) is a helper type for picking apart values. This
#   is always 64 bits long, with a variant for each address size (32 bits
#   or 64 bits) and endianness (little- or big-endian).
#
#   jsval_layout is a union with 'asBits', 'asDouble', and 'asPtr'
#   branches, and an 's' branch, which is a struct that tries to break out
#   the bitfields a little for the non-double types. On 64-bit machines,
#   jsval_layout also has an 'asUIntPtr' branch.
#
#   On 32-bit platforms, the 's' structure has a 'tag' member at the
#   exponent end of the 's' struct, and a 'payload' union at the mantissa
#   end. The 'payload' union's branches are things like JSString *,
#   JSObject *, and so on: the natural representations of the tags.
#
#   On 64-bit platforms, the payload is 47 bits long; since C++ doesn't let
#   us declare bitfields that hold unions, we can't break it down so
#   neatly. In this case, we apply bit-shifting tricks to the 'asBits'
#   branch of the union to extract the tag.

class Box(object):
    def __init__(self, asBits, jtc):
        self.asBits = asBits
        self.jtc = jtc
        # jsval_layout::asBits is uint64, but somebody botches the sign bit, even
        # though Python integers are arbitrary precision.
        if self.asBits < 0:
            self.asBits = self.asBits + (1 << 64)

    # Return this value's type tag.
    def tag(self): raise NotImplementedError

    # Return this value as a 32-bit integer, double, or address.
    def as_uint32(self): raise NotImplementedError
    def as_double(self): raise NotImplementedError
    def as_address(self): raise NotImplementedError

# Packed non-number boxing --- the format used on x86_64. It would be nice to simply
# call JSVAL_TO_INT, etc. here, but the debugger is likely to see many jsvals, and
# doing several inferior calls for each one seems like a bad idea.
class Punbox(Box):

    FULL_WIDTH     = 64
    TAG_SHIFT      = 47
    PAYLOAD_MASK   = (1 << TAG_SHIFT) - 1
    TAG_MASK       = (1 << (FULL_WIDTH - TAG_SHIFT)) - 1
    TAG_MAX_DOUBLE = 0x1fff0
    TAG_TYPE_MASK  = 0x0000f

    def tag(self):
        tag = self.asBits >> Punbox.TAG_SHIFT
        if tag <= Punbox.TAG_MAX_DOUBLE:
            return self.jtc.DOUBLE
        else:
            return tag & Punbox.TAG_TYPE_MASK

    def as_uint32(self): return int(self.asBits & ((1 << 32) - 1))
    def as_address(self): return gdb.Value(self.asBits & Punbox.PAYLOAD_MASK)

class Nunbox(Box):
    TAG_SHIFT      = 32
    TAG_CLEAR      = 0xffff0000
    PAYLOAD_MASK   = 0xffffffff
    TAG_TYPE_MASK  = 0x0000000f

    def tag(self):
        tag = self.asBits >> Nunbox.TAG_SHIFT
        if tag < Nunbox.TAG_CLEAR:
            return self.jtc.DOUBLE
        return tag & Nunbox.TAG_TYPE_MASK

    def as_uint32(self): return int(self.asBits & Nunbox.PAYLOAD_MASK)
    def as_address(self): return gdb.Value(self.asBits & Nunbox.PAYLOAD_MASK)

# Cache information about the jsval type for this objfile.
class jsvalTypeCache(object):
    def __init__(self, cache):
        # Capture the tag values.
        d = gdb.types.make_enum_dict(gdb.lookup_type('JSValueType'))
        self.DOUBLE    = d['JSVAL_TYPE_DOUBLE']
        self.INT32     = d['JSVAL_TYPE_INT32']
        self.UNDEFINED = d['JSVAL_TYPE_UNDEFINED']
        self.BOOLEAN   = d['JSVAL_TYPE_BOOLEAN']
        self.MAGIC     = d['JSVAL_TYPE_MAGIC']
        self.STRING    = d['JSVAL_TYPE_STRING']
        self.NULL      = d['JSVAL_TYPE_NULL']
        self.OBJECT    = d['JSVAL_TYPE_OBJECT']

        # Let self.magic_names be an array whose i'th element is the name of
        # the i'th magic value.
        d = gdb.types.make_enum_dict(gdb.lookup_type('JSWhyMagic'))
        self.magic_names = list(range(max(d.values()) + 1))
        for (k,v) in d.items(): self.magic_names[v] = k

        # Choose an unboxing scheme for this architecture.
        self.boxer = Punbox if cache.void_ptr_t.sizeof == 8 else Nunbox

@pretty_printer('jsval_layout')
class jsval_layout(object):
    def __init__(self, value, cache):
        # Save the generic typecache, and create our own, if we haven't already.
        self.cache = cache
        if not cache.mod_jsval:
            cache.mod_jsval = jsvalTypeCache(cache)
        self.jtc = cache.mod_jsval

        self.value = value
        self.box = self.jtc.boxer(value['asBits'], self.jtc)

    def to_string(self):
        tag = self.box.tag()
        if tag == self.jtc.INT32:
            value = self.box.as_uint32()
            signbit = 1 << 31
            value = (value ^ signbit) - signbit
        elif tag == self.jtc.UNDEFINED:
            return 'JSVAL_VOID'
        elif tag == self.jtc.BOOLEAN:
            return 'JSVAL_TRUE' if self.box.as_uint32() else 'JSVAL_FALSE'
        elif tag == self.jtc.MAGIC:
            value = self.box.as_uint32()
            if 0 <= value and value < len(self.jtc.magic_names):
                return '$jsmagic(%s)' % (self.jtc.magic_names[value],)
            else:
                return '$jsmagic(%d)' % (value,)
        elif tag == self.jtc.STRING:
            value = self.box.as_address().cast(self.cache.JSString_ptr_t)
        elif tag == self.jtc.NULL:
            return 'JSVAL_NULL'
        elif tag == self.jtc.OBJECT:
            value = self.box.as_address().cast(self.cache.JSObject_ptr_t)
        elif tag == self.jtc.DOUBLE:
            value = self.value['asDouble']
        else:
            return '$jsval(unrecognized!)'
        return '$jsval(%s)' % (value,)

@pretty_printer('JS::Value')
class JSValue(object):
    def __new__(cls, value, cache):
        return jsval_layout(value['data'], cache)
