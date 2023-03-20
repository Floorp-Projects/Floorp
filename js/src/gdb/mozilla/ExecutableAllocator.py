# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

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


class jsjitExecutableAllocatorCache(object):
    """Cache information about the ExecutableAllocator type for this objfile."""

    def __init__(self):
        self.d = None

    def __getattr__(self, name):
        if self.d is None:
            self.initialize()
        return self.d[name]

    def initialize(self):
        self.d = {}
        self.d["ExecutableAllocator"] = gdb.lookup_type("js::jit::ExecutableAllocator")
        self.d["ExecutablePool"] = gdb.lookup_type("js::jit::ExecutablePool")
        self.d["HashNumber"] = gdb.lookup_type("mozilla::HashNumber")


@pretty_printer("js::jit::ExecutableAllocator")
class jsjitExecutableAllocator(object):
    def __init__(self, value, cache):
        if not cache.mod_ExecutableAllocator:
            cache.mod_ExecutableAllocator = jsjitExecutableAllocatorCache()
        self.value = value
        self.cache = cache.mod_ExecutableAllocator

    def to_string(self):
        return "ExecutableAllocator([%s])" % ", ".join([str(x) for x in self])

    def __iter__(self):
        return self.PoolIterator(self)

    class PoolIterator(object):
        def __init__(self, allocator):
            self.allocator = allocator
            self.entryType = allocator.cache.ExecutablePool.pointer()
            self.hashNumType = allocator.cache.HashNumber
            # Emulate the HashSet::Range
            self.table = allocator.value["m_pools"]["mImpl"]["mTable"]
            self.index = 0
            kHashNumberBits = 32
            hashShift = allocator.value["m_pools"]["mImpl"]["mHashShift"]
            self.capacity = 1 << (kHashNumberBits - hashShift)
            if self.table == 0:
                self.capacity = 0
            # auto hashes = reinterpret_cast<HashNumber*>(mTable);
            self.hashes = self.table.cast(self.hashNumType.pointer())
            # auto entries = reinterpret_cast<Entry*>(&hashes[capacity()]);
            self.entries = (self.hashes + self.capacity).cast(self.entryType.pointer())

        def __iter__(self):
            return self

        def next(self):
            return self.__next__()

        def __next__(self):
            cur = self.index
            if cur >= self.capacity:
                raise StopIteration()
            self.index = self.index + 1
            if self.hashes[cur] > 1:  # table[i]->isLive()
                return self.entries[cur]
            return self.__next__()


@ptr_pretty_printer("js::jit::ExecutablePool")
class jsjitExecutablePool(mozilla.prettyprinters.Pointer):
    def __init__(self, value, cache):
        if not cache.mod_ExecutableAllocator:
            cache.mod_ExecutableAllocator = jsjitExecutableAllocatorCache()
        self.value = value
        self.cache = cache.mod_ExecutableAllocator

    def to_string(self):
        pages = self.value["m_allocation"]["pages"]
        size = self.value["m_allocation"]["size"]
        return "ExecutablePool %08x-%08x" % (pages, pages + size)
