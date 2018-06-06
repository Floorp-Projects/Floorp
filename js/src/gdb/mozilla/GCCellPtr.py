# Pretty-printers for GCCellPtr values.

import gdb
import mozilla.prettyprinters

from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Cache information about the JS::TraceKind type for this objfile.


class GCCellPtrTypeCache(object):
    def __init__(self, cache):
        self.TraceKind_t = gdb.lookup_type('JS::TraceKind')

        # Build a mapping from TraceKind enum values to the types they denote.
        e = gdb.types.make_enum_dict(self.TraceKind_t)
        kind_to_type = {}

        def kind(k, t):
            kind_to_type[e['JS::TraceKind::' + k]] = gdb.lookup_type(t)
        kind('Object',      'JSObject')
        kind('String',      'JSString')
        kind('Symbol',      'JS::Symbol')
        kind('Script',      'JSScript')
        kind('Shape',       'js::Shape')
        kind('ObjectGroup', 'js::ObjectGroup')
        kind('BaseShape',   'js::BaseShape')
        kind('JitCode',     'js::jit::JitCode')
        kind('LazyScript',  'js::LazyScript')
        self.kind_to_type = kind_to_type

        self.Null = e['JS::TraceKind::Null']
        self.mask = gdb.parse_and_eval('JS::OutOfLineTraceKindMask')


@pretty_printer('JS::GCCellPtr')
class GCCellPtr(object):
    def __init__(self, value, cache):
        self.value = value
        if not cache.mod_GCCellPtr:
            cache.mod_GCCellPtr = GCCellPtrTypeCache(cache)
        self.cache = cache

    def to_string(self):
        ptr = self.value['ptr']
        kind = ptr & self.cache.mod_GCCellPtr.mask
        if kind == self.cache.mod_GCCellPtr.Null:
            return "JS::GCCellPtr(nullptr)"
        tipe = self.cache.mod_GCCellPtr.kind_to_type[int(kind)]
        return "JS::GCCellPtr(({}*) {})".format(tipe, ptr.cast(self.cache.void_ptr_t))
