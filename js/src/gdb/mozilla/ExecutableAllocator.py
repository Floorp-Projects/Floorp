"""
All jitted code is allocated via the ExecutableAllocator class. Make GDB aware
of them, such that we can query for pages which are containing code which are
allocated by the Jits.
"""

import gdb
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, ptr_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Cache information about the JSString type for this objfile.
class jsjitExecutableAllocatorCache(object):
    def __init__(self):
        self.d = None

    def __getattr__(self, name):
        if self.d is None:
            self.initialize()
        return self.d[name]

    def initialize(self):
        self.d = {}
        self.d['ExecutableAllocator'] = gdb.lookup_type('js::jit::ExecutableAllocator')
        self.d['ExecutablePool'] = gdb.lookup_type('js::jit::ExecutablePool')

@pretty_printer("js::jit::ExecutableAllocator")
class jsjitExecutableAllocator(object):
    def __init__(self, value, cache):
        if not cache.mod_ExecutableAllocator:
            cache.mod_ExecutableAllocator = jsjitExecutableAllocatorCache()
        self.value = value
        self.cache = cache.mod_ExecutableAllocator

    def to_string(self):
        return "ExecutableAllocator([%s])" % ', '.join([str(x) for x in self])

    def __iter__(self):
        return self.PoolIterator(self)

    class PoolIterator(object):
        def __init__(self, allocator):
            self.allocator = allocator
            self.entryType = allocator.cache.ExecutablePool.pointer()
            # Emulate the HashSet::Range
            self.table = allocator.value['m_pools']['impl']['table']
            self.index = 0;
            HASHNUMBER_BIT_SIZE =  32
            self.max = 1 << (HASHNUMBER_BIT_SIZE - allocator.value['m_pools']['impl']['hashShift'])
            if self.table == 0:
                self.max = 0

        def __iter__(self):
            return self;

        def next(self):
            cur = self.index
            if cur >= self.max:
                raise StopIteration()
            self.index = self.index + 1
            if self.table[cur]['keyHash'] > 1: # table[i]->isLive()
                return self.table[cur]['mem']['u']['mDummy'].cast(self.entryType)
            return self.next()

@ptr_pretty_printer("js::jit::ExecutablePool")
class jsjitExecutablePool(mozilla.prettyprinters.Pointer):
    def __init__(self, value, cache):
        if not cache.mod_ExecutableAllocator:
            cache.mod_ExecutableAllocator = jsjitExecutableAllocatorCache()
        self.value = value
        self.cache = cache.mod_ExecutableAllocator

    def to_string(self):
        pages = self.value['m_allocation']['pages']
        size = self.value['m_allocation']['size']
        return "ExecutablePool %08x-%08x" % (pages, pages + size)
