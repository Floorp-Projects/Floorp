# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a gdb extension to automate the process of tracing backwards in rr
# from a jit instruction to the code that generated that instruction.
#
# Usage:
#  (rr) x/i $pc
#  => 0x240e954ac13a:	pushq  (%rbx)
#  (rr) jitsrc 0x240e954ac13a

import gdb
import re

# (base_name, hops, func_name, source_var, dest_var) tuples, such that :
#   - `base_name`: a regex matching the name of the function that implements
#       the actual write
#   - `hops`: the number of stack frames between `base_name` and `func_name`
#   - `func_name`: a regex matching the name of the function that calls memcpy
#   - `source_var`: an expression that can be evaluated in the frame
#       corresponding to `func_name` to get the source of the memcpy
#   - `dest_var`: an expression that can be evaluated in the frame
#       corresponding to `func_name` to get the destination of the memcpy
#
# If an invocation of `jitsrc` stops in the middle of a memcpy, the solution
# is normally to add a new pattern here.
patterns = [
    (
        "__memmove_avx_unaligned_erms",
        1,
        "js::jit::X86Encoding::BaseAssembler::executableCopy",
        "src",
        "dst",
    ),
    (
        "__memcpy_avx_unaligned",
        1,
        "js::jit::X86Encoding::BaseAssembler::executableCopy",
        "src",
        "dst",
    ),
    ("__memmove_avx_unaligned_erms", 1, "arena_t::RallocSmallOrLarge", "aPtr", "ret"),
    ("__memcpy_avx_unaligned", 1, "arena_t::RallocSmallOrLarge", "aPtr", "ret"),
    (
        "mozilla::detail::VectorImpl<.*>::new_<.*>",
        3,
        "mozilla::Vector<.*>::convertToHeapStorage",
        "beginNoCheck()",
        "newBuf",
    ),
    (
        "__memmove_avx_unaligned_erms",
        1,
        "js::jit::AssemblerBufferWithConstantPools",
        "&cur->instructions[0]",
        "dest",
    ),
    (
        "__memcpy_sse2_unaligned",
        1,
        "js::jit::AssemblerBufferWithConstantPools",
        "&cur->instructions[0]",
        "dest",
    ),
    (
        "__memcpy_sse2_unaligned",
        2,
        "js::jit::AssemblerX86Shared::executableCopy",
        "masm.m_formatter.m_buffer.m_buffer.mBegin",
        "buffer",
    ),
    ("__memcpy_sse2_unaligned", 1, "arena_t::RallocSmallOrLarge", "aPtr", "ret"),
    ("js::jit::X86Encoding::SetInt32", 0, "js::jit::X86Encoding::SetInt32", "0", "0"),
    (
        "js::jit::X86Encoding::SetPointer",
        0,
        "js::jit::X86Encoding::SetPointer",
        "0",
        "0",
    ),
    (
        "<unnamed>",
        1,
        "js::jit::AssemblerBufferWithConstantPools<.*>::executableCopy",
        "&cur->instructions[0]",
        "dest",
    ),
    ("std::__copy_move", 4, "CopySpan", "source.data()", "target.data()"),
    (
        "__memmove_avx_unaligned_erms",
        1,
        "mozilla::detail::EndianUtils::copyAndSwapTo<.*0,.*0",
        "aSrc",
        "(size_t) aDest",
    ),
]


class JitSource(gdb.Command):
    def __init__(self):
        super(JitSource, self).__init__("jitsrc", gdb.COMMAND_RUNNING)
        self.dont_repeat()

    def disable_breakpoints(self):
        self.disabled_breakpoints = [b for b in gdb.breakpoints() if b.enabled]
        for b in self.disabled_breakpoints:
            b.enabled = False

    def enable_breakpoints(self):
        for b in self.disabled_breakpoints:
            b.enabled = True

    def search_stack(self, base_name, hops, name, src, dst, address):
        current_frame_name = gdb.newest_frame().name() or "<unnamed>"
        if not re.match(base_name, current_frame_name):
            return None
        f = gdb.newest_frame()
        for _ in range(hops):
            f = f.older()
        if not re.match(name, f.name()):
            return None
        f.select()
        src_val = gdb.parse_and_eval(src)
        dst_val = gdb.parse_and_eval(dst)
        return hex(src_val + int(address, 16) - dst_val)

    def next_address(self, old):
        for pattern in patterns:
            found = self.search_stack(*pattern, old)
            if found:
                return found
        return None

    def runback(self, address):
        b = gdb.Breakpoint(
            "*" + address, type=gdb.BP_WATCHPOINT, wp_class=gdb.WP_WRITE, internal=True
        )
        while b.hit_count == 0:
            gdb.execute("rc", to_string=True)
        b.delete()

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        address = args[0]
        self.disable_breakpoints()
        while address:
            self.runback(address)
            address = self.next_address(address)
        self.enable_breakpoints()


# Register to the gdb runtime
JitSource()
