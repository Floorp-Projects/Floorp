# mozilla/prettyprinters.py --- infrastructure for SpiderMonkey's auto-loaded pretty-printers.

import gdb
import re

# Decorators for declaring pretty-printers.
#
# In each case, the decoratee should be a SpiderMonkey-style pretty-printer
# factory, taking both a gdb.Value instance and a TypeCache instance as
# arguments; see TypeCache, below.

# Check that |fn| hasn't been registered as a pretty-printer under some
# other name already. (The 'enabled' flags used by GDB's
# 'enable/disable/info pretty-printer' commands are simply stored as
# properties of the function objects themselves, so a single function
# object can't carry the 'enabled' flags for two different printers.)
def check_for_reused_pretty_printer(fn):
    if hasattr(fn, 'enabled'):
        raise RuntimeError("pretty-printer function %r registered more than once" % fn)

# a dictionary mapping gdb.Type tags to pretty-printer functions.
printers_by_tag = {}

# A decorator: add the decoratee as a pretty-printer lookup function for types
# named |type_name|.
def pretty_printer(type_name):
    def add(fn):
        check_for_reused_pretty_printer(fn)
        add_to_subprinter_list(fn, type_name)
        printers_by_tag[type_name] = fn
        return fn
    return add

# a dictionary mapping gdb.Type tags to pretty-printer functions for pointers to
# that type.
ptr_printers_by_tag = {}

# A decorator: add the decoratee as a pretty-printer lookup function for
# pointers to types named |type_name|.
def ptr_pretty_printer(type_name):
    def add(fn):
        check_for_reused_pretty_printer(fn)
        add_to_subprinter_list(fn, "ptr-to-" + type_name)
        ptr_printers_by_tag[type_name] = fn
        return fn
    return add

# a dictionary mapping gdb.Type tags to pretty-printer functions for
# references to that type.
ref_printers_by_tag = {}

# A decorator: add the decoratee as a pretty-printer lookup function for
# references to instances of types named |type_name|.
def ref_pretty_printer(type_name):
    def add(fn):
        check_for_reused_pretty_printer(fn)
        add_to_subprinter_list(fn, "ref-to-" + type_name)
        ref_printers_by_tag[type_name] = fn
        return fn
    return add

# a dictionary mapping the template name portion of gdb.Type tags to
# pretty-printer functions for instantiations of that template.
template_printers_by_tag = {}

# A decorator: add the decoratee as a pretty-printer lookup function for
# instantiations of templates named |template_name|.
def template_pretty_printer(template_name):
    def add(fn):
        check_for_reused_pretty_printer(fn)
        add_to_subprinter_list(fn, 'instantiations-of-' + template_name)
        template_printers_by_tag[template_name] = fn
        return fn
    return add

# A list of (REGEXP, PRINTER) pairs, such that if REGEXP (a RegexObject)
# matches the result of converting a gdb.Value's type to a string, then
# PRINTER is a pretty-printer lookup function that will probably like that
# value.
printers_by_regexp = []

# A decorator: add the decoratee as a pretty-printer factory for types
# that, when converted to a string, match |pattern|. Use |name| as the
# pretty-printer's name, when listing, enabling and disabling.
def pretty_printer_for_regexp(pattern, name):
    compiled = re.compile(pattern)
    def add(fn):
        check_for_reused_pretty_printer(fn)
        add_to_subprinter_list(fn, name)
        printers_by_regexp.append((compiled, fn))
        return fn
    return add

# Forget all pretty-printer lookup functions defined in the module name
# |module_name|, if any exist. Use this at the top of each pretty-printer
# module like this:
#
#   clear_module_printers(__name__)
def clear_module_printers(module_name):
    global printers_by_tag, ptr_printers_by_tag, ref_printers_by_tag
    global template_printers_by_tag, printers_by_regexp

    # Remove all pretty-printers defined in the module named |module_name|
    # from d.
    def clear_dictionary(d):
        # Walk the dictionary, building a list of keys whose entries we
        # should remove. (It's not safe to delete entries from a dictionary
        # while we're iterating over it.)
        to_delete = []
        for (k, v) in d.items():
            if v.__module__ == module_name:
                to_delete.append(k)
                remove_from_subprinter_list(v)
        for k in to_delete:
            del d[k]

    clear_dictionary(printers_by_tag)
    clear_dictionary(ptr_printers_by_tag)
    clear_dictionary(ref_printers_by_tag)
    clear_dictionary(template_printers_by_tag)

    # Iterate over printers_by_regexp, deleting entries from the given module.
    new_list = []
    for p in printers_by_regexp:
        if p.__module__ == module_name:
            remove_from_subprinter_list(p)
        else:
            new_list.append(p)
    printers_by_regexp = new_list

# Our subprinters array. The 'subprinters' attributes of all lookup
# functions returned by lookup_for_objfile point to this array instance,
# which we mutate as subprinters are added and removed.
subprinters = []

# Set up the 'name' and 'enabled' attributes on |subprinter|, and add it to our
# list of all SpiderMonkey subprinters.
def add_to_subprinter_list(subprinter, name):
    subprinter.name = name
    subprinter.enabled = True
    subprinters.append(subprinter)

# Remove |subprinter| from our list of all SpiderMonkey subprinters.
def remove_from_subprinter_list(subprinter):
    subprinters.remove(subprinter)

# An exception class meaning, "This objfile has no SpiderMonkey in it."
class NotSpiderMonkeyObjfileError(TypeError):
    pass

# TypeCache: a cache for frequently used information about an objfile.
#
# When a new SpiderMonkey objfile is loaded, we construct an instance of
# this class for it. Then, whenever we construct a pretty-printer for some
# gdb.Value, we also pass, as a second argument, the TypeCache for the
# objfile to which that value's type belongs.
#
# if objfile doesn't seem to have SpiderMonkey code in it, the constructor
# raises NotSpiderMonkeyObjfileError.
#
# Pretty-printer modules may add attributes to this to hold their own
# cached values. Such attributes should be named mod_NAME, where the module
# is named mozilla.NAME; for example, mozilla.JSString should store its
# metadata in the TypeCache's mod_JSString attribute.
class TypeCache(object):
    def __init__(self, objfile):
        self.objfile = objfile

        # Unfortunately, the Python interface doesn't allow us to specify
        # the objfile in whose scope lookups should occur. But simply
        # knowing that we need to lookup the types afresh is probably
        # enough.
        self.void_t = gdb.lookup_type('void')
        self.void_ptr_t = self.void_t.pointer()
        try:
            self.JSString_ptr_t = gdb.lookup_type('JSString').pointer()
            self.JSSymbol_ptr_t = gdb.lookup_type('JS::Symbol').pointer()
            self.JSObject_ptr_t = gdb.lookup_type('JSObject').pointer()
        except gdb.error:
            raise NotSpiderMonkeyObjfileError

        self.mod_GCCellPtr = None
        self.mod_Interpreter = None
        self.mod_JSObject = None
        self.mod_JSString = None
        self.mod_jsval = None
        self.mod_ExecutableAllocator = None
        self.mod_IonGraph = None

# Yield a series of all the types that |t| implements, by following typedefs
# and iterating over base classes. Specifically:
# - |t| itself is the first value yielded.
# - If we yield a typedef, we later yield its definition.
# - If we yield a type with base classes, we later yield those base classes.
# - If we yield a type with some base classes that are typedefs,
#   we yield all the type's base classes before following the typedefs.
#   (Actually, this never happens, because G++ doesn't preserve the typedefs in
#   the DWARF.)
#
# This is a hokey attempt to order the implemented types by meaningfulness when
# pretty-printed. Perhaps it is entirely misguided, and we should actually
# collect all applicable pretty-printers, and then use some ordering on the
# pretty-printers themselves.
#
# We may yield a type more than once (say, if it appears more than once in the
# class hierarchy).
def implemented_types(t):

    # Yield all types that follow |t|.
    def followers(t):
        if t.code == gdb.TYPE_CODE_TYPEDEF:
            yield t.target()
            for t2 in followers(t.target()): yield t2
        elif t.code == gdb.TYPE_CODE_STRUCT:
            base_classes = []
            for f in t.fields():
                if f.is_base_class:
                    yield f.type
                    base_classes.append(f.type)
            for b in base_classes:
                for t2 in followers(b): yield t2

    yield t
    for t2 in followers(t): yield t2

template_regexp = re.compile("([\w_:]+)<")

# Construct and return a pretty-printer lookup function for objfile, or
# return None if the objfile doesn't contain SpiderMonkey code
# (specifically, definitions for SpiderMonkey types).
def lookup_for_objfile(objfile):
    # Create a type cache for this objfile.
    try:
        cache = TypeCache(objfile)
    except NotSpiderMonkeyObjfileError:
        if gdb.parameter("verbose"):
            gdb.write("objfile '%s' has no SpiderMonkey code; not registering pretty-printers\n"
                      % (objfile.filename,))
        return None

    # Return a pretty-printer for |value|, if we have one. This is the lookup
    # function object we place in each gdb.Objfile's pretty-printers list, so it
    # carries |name|, |enabled|, and |subprinters| attributes.
    def lookup(value):
        # If |table| has a pretty-printer for |tag|, apply it to |value|.
        def check_table(table, tag):
            if tag in table:
                f = table[tag]
                if f.enabled:
                    return f(value, cache)
            return None

        def check_table_by_type_name(table, t):
            if t.code == gdb.TYPE_CODE_TYPEDEF:
                return check_table(table, str(t))
            elif t.code == gdb.TYPE_CODE_STRUCT and t.tag:
                return check_table(table, t.tag)
            else:
                return None

        for t in implemented_types(value.type):
            if t.code == gdb.TYPE_CODE_PTR:
                for t2 in implemented_types(t.target()):
                    p = check_table_by_type_name(ptr_printers_by_tag, t2)
                    if p: return p
            elif t.code == gdb.TYPE_CODE_REF:
                for t2 in implemented_types(t.target()):
                    p = check_table_by_type_name(ref_printers_by_tag, t2)
                    if p: return p
            else:
                p = check_table_by_type_name(printers_by_tag, t)
                if p: return p
                if t.code == gdb.TYPE_CODE_STRUCT and t.tag:
                    m = template_regexp.match(t.tag)
                    if m:
                        p = check_table(template_printers_by_tag, m.group(1))
                        if p: return p

        # Failing that, look for a printer in printers_by_regexp. We have
        # to scan the whole list, so regexp printers should be used
        # sparingly.
        s = str(value.type)
        for (r, f) in printers_by_regexp:
            if f.enabled:
                m = r.match(s)
                if m:
                    p = f(value, cache)
                    if p: return p

        # No luck.
        return None

    # Give |lookup| the attributes expected of a pretty-printer with
    # subprinters, for enabling and disabling.
    lookup.name = "SpiderMonkey"
    lookup.enabled = True
    lookup.subprinters = subprinters

    return lookup

# A base class for pretty-printers for pointer values that handles null
# pointers, by declining to construct a pretty-printer for them at all.
# Derived classes may simply assume that self.value is non-null.
#
# To help share code, this class can also be used with reference types.
#
# This class provides the following methods, which subclasses are free to
# override:
#
# __init__(self, value, cache): Save value and cache as properties by those names
#     on the instance.
#
# to_string(self): format the type's name and address, as GDB would, and then
#     call a 'summary' method (which the subclass must define) to produce a
#     description of the referent.
#
#     Note that pretty-printers returning a 'string' display hint must not use
#     this default 'to_string' method, as GDB will take everything it returns,
#     including the type name and address, as string contents.
class Pointer(object):
    def __new__(cls, value, cache):
        # Don't try to provide pretty-printers for NULL pointers.
        if value.type.strip_typedefs().code == gdb.TYPE_CODE_PTR and value == 0:
            return None
        return super(Pointer, cls).__new__(cls)

    def __init__(self, value, cache):
        self.value = value
        self.cache = cache

    def to_string(self):
        # See comment above.
        assert not hasattr(self, 'display_hint') or self.display_hint() != 'string'
        concrete_type = self.value.type.strip_typedefs()
        if concrete_type.code == gdb.TYPE_CODE_PTR:
            address = self.value.cast(self.cache.void_ptr_t)
        elif concrete_type.code == gdb.TYPE_CODE_REF:
            address = '@' + str(self.value.address.cast(self.cache.void_ptr_t))
        else:
            assert not "mozilla.prettyprinters.Pointer applied to bad value type"
        try:
            summary = self.summary()
        except gdb.MemoryError as r:
            summary = str(r)
        v = '(%s) %s %s' % (self.value.type, address, summary)
        return v

    def summary(self):
        raise NotImplementedError

field_enum_value = None

# Given |t|, a gdb.Type instance representing an enum type, return the
# numeric value of the enum value named |name|.
#
# Pre-2012-4-18 versions of GDB store the value of an enum member on the
# gdb.Field's 'bitpos' attribute; later versions store it on the 'enumval'
# attribute. This function retrieves the value from either.
def enum_value(t, name):
    global field_enum_value
    f = t[name]
    # Monkey-patching is a-okay in polyfills! Just because.
    if not field_enum_value:
        if hasattr(f, 'enumval'):
            field_enum_value = lambda f: f.enumval
        else:
            field_enum_value = lambda f: f.bitpos
    return field_enum_value(f)
