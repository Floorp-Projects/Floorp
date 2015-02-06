# Pretty-printers for SpiderMonkey JSObjects.

import gdb
import mozilla.JSString
import mozilla.prettyprinters as prettyprinters
from mozilla.prettyprinters import ptr_pretty_printer, ref_pretty_printer
from mozilla.Root import deref

prettyprinters.clear_module_printers(__name__)

class JSObjectTypeCache(object):
    def __init__(self, value, cache):
        # In GDB 7.4.50, with some programs, this first dummy gdb.lookup_type
        # call is required to make the second lookup_type call succeed. A GDB
        # built from the public sources as of 2012-12-12 did not require the
        # dummy lookup.
        gdb.lookup_type('js::BaseShape')
        baseshape_flags = gdb.lookup_type('js::BaseShape::Flag')
        self.flag_DELEGATE = prettyprinters.enum_value(baseshape_flags, 'js::BaseShape::DELEGATE')
        self.func_ptr_type = gdb.lookup_type('JSFunction').pointer()

# There should be no need to register this for JSFunction as well, since we
# search for pretty-printers under the names of base classes, and
# JSFunction has JSObject as a base class.

@ptr_pretty_printer('JSObject')
class JSObjectPtrOrRef(prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(JSObjectPtrOrRef, self).__init__(value, cache)
        if not cache.mod_JSObject:
            cache.mod_JSObject = JSObjectTypeCache(value, cache)
        self.otc = cache.mod_JSObject

    def summary(self):
        shape = deref(self.value['shape_'])
        baseshape = deref(shape['base_'])
        group = deref(self.value['group_'])
        class_name = group['clasp_']['name'].string()
        flags = baseshape['flags']
        is_delegate = bool(flags & self.otc.flag_DELEGATE)
        name = None
        if class_name == 'Function':
            function = self.value
            concrete_type = function.type.strip_typedefs()
            if concrete_type.code == gdb.TYPE_CODE_REF:
                function = function.address
            function = function.cast(self.otc.func_ptr_type)
            atom = deref(function['atom_'])
            name = str(atom) if atom else '<unnamed>'
        return '[object %s%s]%s' % (class_name,
                                    ' ' + name if name else '',
                                    ' delegate' if is_delegate else '')

@ref_pretty_printer('JSObject')
def JSObjectRef(value, cache): return JSObjectPtrOrRef(value, cache)
