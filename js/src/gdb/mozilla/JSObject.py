# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printers for SpiderMonkey JSObjects.

import re
import gdb
import mozilla.prettyprinters as prettyprinters
from mozilla.jsval import JSValue
from mozilla.prettyprinters import ptr_pretty_printer, ref_pretty_printer
from mozilla.CellHeader import get_header_ptr

prettyprinters.clear_module_printers(__name__)


class JSObjectTypeCache(object):
    def __init__(self):
        object_flag = gdb.lookup_type("js::ObjectFlag")
        self.objectflag_IsUsedAsPrototype = prettyprinters.enum_value(
            object_flag, "js::ObjectFlag::IsUsedAsPrototype"
        )
        self.value_ptr_t = gdb.lookup_type("JS::Value").pointer()
        self.func_ptr_t = gdb.lookup_type("JSFunction").pointer()
        self.class_NON_NATIVE = gdb.parse_and_eval("JSClass::NON_NATIVE")
        self.BaseShape_ptr_t = gdb.lookup_type("js::BaseShape").pointer()
        self.Shape_ptr_t = gdb.lookup_type("js::Shape").pointer()
        self.JSClass_ptr_t = gdb.lookup_type("JSClass").pointer()
        self.JSScript_ptr_t = gdb.lookup_type("JSScript").pointer()
        self.JSFunction_AtomSlot = gdb.parse_and_eval("JSFunction::AtomSlot")
        self.JSFunction_NativeJitInfoOrInterpretedScriptSlot = gdb.parse_and_eval(
            "JSFunction::NativeJitInfoOrInterpretedScriptSlot"
        )


# There should be no need to register this for JSFunction as well, since we
# search for pretty-printers under the names of base classes, and
# JSFunction has JSObject as a base class.


gdb_string_regexp = re.compile(r'(?:0x[0-9a-z]+ )?(?:<.*> )?"(.*)"', re.I)


@ptr_pretty_printer("JSObject")
class JSObjectPtrOrRef(prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(JSObjectPtrOrRef, self).__init__(value, cache)
        if not cache.mod_JSObject:
            cache.mod_JSObject = JSObjectTypeCache()
        self.otc = cache.mod_JSObject

    def summary(self):
        shape = get_header_ptr(self.value, self.otc.Shape_ptr_t)
        baseshape = get_header_ptr(shape, self.otc.BaseShape_ptr_t)
        classp = get_header_ptr(baseshape, self.otc.JSClass_ptr_t)
        non_native = classp["flags"] & self.otc.class_NON_NATIVE

        # Use GDB to format the class name, but then strip off the address
        # and the quotes.
        class_name = str(classp["name"])
        m = gdb_string_regexp.match(class_name)
        if m:
            class_name = m.group(1)

        if non_native:
            return "[object {}]".format(class_name)
        else:
            flags = shape["objectFlags_"]["flags_"]
            used_as_prototype = bool(flags & self.otc.objectflag_IsUsedAsPrototype)
            name = None
            if class_name == "Function":
                function = self.value
                concrete_type = function.type.strip_typedefs()
                if concrete_type.code == gdb.TYPE_CODE_REF:
                    function = function.address
                name = get_function_name(function, self.cache)
            return "[object {}{}]{}".format(
                class_name,
                " " + name if name else "",
                " used_as_prototype" if used_as_prototype else "",
            )


def get_function_name(function, cache):
    if not cache.mod_JSObject:
        cache.mod_JSObject = JSObjectTypeCache()
    otc = cache.mod_JSObject

    function = function.cast(otc.func_ptr_t)
    fixed_slots = (function + 1).cast(otc.value_ptr_t)
    atom_value = JSValue(fixed_slots[otc.JSFunction_AtomSlot], cache)

    if atom_value.is_undefined():
        return "<unnamed>"

    return str(atom_value.get_string())


def get_function_script(function, cache):
    if not cache.mod_JSObject:
        cache.mod_JSObject = JSObjectTypeCache()
    otc = cache.mod_JSObject

    function = function.cast(otc.func_ptr_t)
    fixed_slots = (function + 1).cast(otc.value_ptr_t)
    slot = otc.JSFunction_NativeJitInfoOrInterpretedScriptSlot
    script_value = JSValue(fixed_slots[slot], cache)

    if script_value.is_undefined():
        return 0

    return script_value.get_private()


@ref_pretty_printer("JSObject")
def JSObjectRef(value, cache):
    return JSObjectPtrOrRef(value, cache)
