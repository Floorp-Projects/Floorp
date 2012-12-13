# Pretty-printers and utilities for SpiderMonkey rooting templates:
# Rooted, Handle, MutableHandle, etc.

import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, template_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Common base class for all the rooting template pretty-printers. All these
# templates have one member holding the referent (or a pointer to it), so
# there's not much to it.
class Common(object):
    # The name of the template member holding the referent.
    member = 'ptr'

    # If True, this is a handle type, and should be dereferenced. If False,
    # the template member holds the referent directly.
    handle = False

    def __init__(self, value, cache):
        self.value = value
    def to_string(self):
        ptr = self.value[self.member]
        if self.handle:
            ptr = ptr.dereference()
        # As of 2012-11, GDB suppresses printing pointers in replacement values;
        # see http://sourceware.org/ml/gdb/2012-11/msg00055.html That means that
        # simply returning the 'ptr' member won't work. Instead, just invoke
        # GDB's formatter ourselves.
        return str(ptr)

@template_pretty_printer("js::Rooted")
class Rooted(Common):
    pass

@template_pretty_printer("JS::Handle")
class Handle(Common):
    handle = True

@template_pretty_printer("JS::MutableHandle")
class MutableHandle(Common):
    handle = True

@template_pretty_printer("js::EncapsulatedPtr")
class EncapsulatedPtr(Common):
    member = 'value'

@pretty_printer("js::EncapsulatedValue")
class EncapsulatedValue(Common):
    member = 'value'

# Return the referent of a HeapPtr, Rooted, or Handle.
def deref(root):
    tag = root.type.strip_typedefs().tag
    if tag.startswith('js::HeapPtr<'):
        return root['value']
    elif tag.startswith('js::Rooted<'):
        return root['ptr']
    elif tag.startswith('js::Handle<'):
        return root['ptr']
    else:
        raise NotImplementedError
