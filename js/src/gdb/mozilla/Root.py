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

    # If True, we should strip typedefs from our referent type. (Rooted<T>
    # uses template magic that gives the referent a noisy type.)
    strip_typedefs = False

    # Initialize a pretty-printer for |value|, using |cache|.
    #
    # If given, |content_printer| is a pretty-printer constructor to use for
    # this handle/root/etc.'s referent. Usually, we can just omit this argument
    # and let GDB choose a pretty-printer for the referent given its type, but
    # when the referent is a typedef of an integral type (say, |jsid| in a
    # non-|DEBUG| build), the GNU toolchain (at least) loses the typedef name,
    # and all we know about the referent is its fundamental integer type ---
    # |JS::Rooted<jsid>|, for example, appears in GDB as |JS::Rooted<long>| ---
    # and we are left with no way to choose a meaningful pretty-printer based on
    # the type of the referent alone. However, because we know that the only
    # integer type for which |JS::Rooted| is likely to be instantiated is
    # |jsid|, we *can* register a pretty-printer constructor for the full
    # instantiation |JS::Rooted<long>|. That constructor creates a |JS::Rooted|
    # pretty-printer, and explicitly specifies the constructor for the referent,
    # using this initializer's |content_printer| argument.
    def __init__(self, value, cache, content_printer=None):
        self.value = value
        self.cache = cache
        self.content_printer = content_printer
    def to_string(self):
        ptr = self.value[self.member]
        if self.handle:
            ptr = ptr.dereference()
        if self.strip_typedefs:
            ptr = ptr.cast(ptr.type.strip_typedefs())
        if self.content_printer:
            return self.content_printer(ptr, self.cache).to_string()
        else:
            # As of 2012-11, GDB suppresses printing pointers in replacement
            # values; see http://sourceware.org/ml/gdb/2012-11/msg00055.html
            # That means that simply returning the 'ptr' member won't work.
            # Instead, just invoke GDB's formatter ourselves.
            return str(ptr)

@template_pretty_printer("JS::Rooted")
class Rooted(Common):
    strip_typedefs = True

@template_pretty_printer("JS::Handle")
class Handle(Common):
    handle = True

@template_pretty_printer("JS::MutableHandle")
class MutableHandle(Common):
    handle = True

@template_pretty_printer("js::BarrieredBase")
class BarrieredBase(Common):
    member = 'value'

# Return the referent of a HeapPtr, Rooted, or Handle.
def deref(root):
    tag = root.type.strip_typedefs().tag
    if not tag:
        raise TypeError("Can't dereference type with no structure tag: %s" % (root.type,))
    elif tag.startswith('js::HeapPtr<'):
        return root['value']
    elif tag.startswith('JS::Rooted<'):
        return root['ptr']
    elif tag.startswith('JS::Handle<'):
        return root['ptr']
    elif tag.startswith('js::GCPtr<'):
        return root['value']
    else:
        raise NotImplementedError("Unrecognized tag: " + tag)
