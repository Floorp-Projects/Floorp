# Pretty-printers for SpiderMonkey JSObjects.

import gdb
import mozilla.JSString
import mozilla.prettyprinters as prettyprinters
from mozilla.prettyprinters import ptr_pretty_printer
from mozilla.Root import deref

prettyprinters.clear_module_printers(__name__)

class JSObjectTypeCache(object):
    def __init__(self, value, cache):
        baseshape_flags = gdb.lookup_type('js::BaseShape::Flag')
        self.flag_DELEGATE = prettyprinters.enum_value(baseshape_flags, 'js::BaseShape::DELEGATE')
        self.func_ptr_type = gdb.lookup_type('JSFunction').pointer()

# There should be no need to register this for JSFunction as well, since we
# search for pretty-printers under the names of base classes, and
# JSFunction has JSObject as a base class.

@ptr_pretty_printer('JSObject')
class JSObjectPtr(prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(JSObjectPtr, self).__init__(value, cache)
        if not cache.mod_JSObject:
            cache.mod_JSObject = JSObjectTypeCache(value, cache)
        self.otc = cache.mod_JSObject

    def summary(self):
        shape = deref(self.value['shape_'])
        baseshape = deref(shape['base_'])
        class_name = baseshape['clasp']['name'].string()
        flags = baseshape['flags']
        is_delegate = bool(flags & self.otc.flag_DELEGATE)
        name = None
        if class_name == 'Function':
            function = self.value.cast(self.otc.func_ptr_type)
            atom = deref(function['atom_'])
            name = str(atom) if atom else '<unnamed>'
        return '[object %s%s]%s' % (class_name,
                                    ' ' + name if name else '',
                                    ' delegate' if is_delegate else '')
