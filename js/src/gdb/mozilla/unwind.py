# mozilla/unwind.py --- unwinder and frame filter for SpiderMonkey

import gdb
import gdb.types
from gdb.FrameDecorator import FrameDecorator
import re
import platform
from mozilla.ExecutableAllocator import jsjitExecutableAllocatorCache, jsjitExecutableAllocator

# For ease of use in Python 2, we use "long" instead of "int"
# everywhere.
try:
    long
except NameError:
    long = int

# The Python 3 |map| built-in works lazily, but in Python 2 we need
# itertools.imap to get this.
try:
    from itertools import imap
except ImportError:
    imap = map

_have_unwinder = True
try:
    from gdb.unwinder import Unwinder
except ImportError:
    _have_unwinder = False
    # We need something here; it doesn't matter what as no unwinder
    # will ever be instantiated.
    Unwinder = object


def debug(something):
    # print("@@ " + something)
    pass


# Maps frametype enum base names to corresponding class.
SizeOfFramePrefix = {
    'JitFrame_IonJS': 'ExitFrameLayout',
    'JitFrame_BaselineJS': 'JitFrameLayout',
    'JitFrame_BaselineStub': 'BaselineStubFrameLayout',
    'JitFrame_IonStub': 'JitStubFrameLayout',
    'JitFrame_CppToJSJit': 'JitFrameLayout',
    'JitFrame_WasmToJSJit': 'JitFrameLayout',
    'JitFrame_Rectifier': 'RectifierFrameLayout',
    'JitFrame_IonAccessorIC': 'IonAccessorICFrameLayout',
    'JitFrame_IonICCall': 'IonICCallFrameLayout',
    'JitFrame_Exit': 'ExitFrameLayout',
    'JitFrame_Bailout': 'JitFrameLayout',
}


class UnwinderTypeCache(object):
    # All types and symbols that we need are attached to an object that we
    # can dispose of as needed.

    def __init__(self):
        self.d = None
        self.frame_enum_names = {}
        self.frame_class_types = {}

    # We take this bizarre approach to defer trying to look up any
    # symbols until absolutely needed.  Without this, the loading
    # approach taken by the gdb-tests would cause spurious exceptions.
    def __getattr__(self, name):
        if self.d is None:
            self.initialize()
        return self.d[name]

    def value(self, name):
        return long(gdb.parse_and_eval('js::jit::' + name))

    def initialize(self):
        self.d = {}
        self.d['FRAMETYPE_MASK'] = (1 << self.value('FRAMETYPE_BITS')) - 1
        self.d['FRAMESIZE_SHIFT'] = self.value('FRAMESIZE_SHIFT')
        self.d['FRAME_HEADER_SIZE_SHIFT'] = self.value('FRAME_HEADER_SIZE_SHIFT')
        self.d['FRAME_HEADER_SIZE_MASK'] = self.value('FRAME_HEADER_SIZE_MASK')

        self.compute_frame_info()
        commonFrameLayout = gdb.lookup_type('js::jit::CommonFrameLayout')
        self.d['typeCommonFrameLayout'] = commonFrameLayout
        self.d['typeCommonFrameLayoutPointer'] = commonFrameLayout.pointer()
        self.d['per_tls_context'] = gdb.lookup_global_symbol('js::TlsContext')

        self.d['void_starstar'] = gdb.lookup_type('void').pointer().pointer()
        self.d['mod_ExecutableAllocator'] = jsjitExecutableAllocatorCache()

        jitframe = gdb.lookup_type("js::jit::JitFrameLayout")
        self.d['jitFrameLayoutPointer'] = jitframe.pointer()

        self.d['CalleeToken_Function'] = self.value("CalleeToken_Function")
        self.d['CalleeToken_FunctionConstructing'] = self.value("CalleeToken_FunctionConstructing")
        self.d['CalleeToken_Script'] = self.value("CalleeToken_Script")
        self.d['JSFunction'] = gdb.lookup_type("JSFunction").pointer()
        self.d['JSScript'] = gdb.lookup_type("JSScript").pointer()
        self.d['Value'] = gdb.lookup_type("JS::Value")

        self.d['SOURCE_SLOT'] = long(gdb.parse_and_eval('js::ScriptSourceObject::SOURCE_SLOT'))
        self.d['NativeObject'] = gdb.lookup_type("js::NativeObject").pointer()
        self.d['HeapSlot'] = gdb.lookup_type("js::HeapSlot").pointer()
        self.d['ScriptSource'] = gdb.lookup_type("js::ScriptSource").pointer()

    # Compute maps related to jit frames.
    def compute_frame_info(self):
        t = gdb.lookup_type('enum js::jit::FrameType')
        for field in t.fields():
            # Strip off "js::jit::".
            name = field.name[9:]
            enumval = long(field.enumval)
            self.d[name] = enumval
            self.frame_enum_names[enumval] = name
            class_type = gdb.lookup_type('js::jit::' + SizeOfFramePrefix[name])
            self.frame_class_types[enumval] = class_type.pointer()

# gdb doesn't have a direct way to tell us if a given address is
# claimed by some shared library or the executable.  See
# https://sourceware.org/bugzilla/show_bug.cgi?id=19288
# In the interest of not requiring a patched gdb, instead we read
# /proc/.../maps.  This only works locally, but maybe could work
# remotely using "remote get".  FIXME.


def parse_proc_maps():
    mapfile = '/proc/' + str(gdb.selected_inferior().pid) + '/maps'
    # Note we only examine executable mappings here.
    matcher = re.compile("^([a-fA-F0-9]+)-([a-fA-F0-9]+)\s+..x.\s+\S+\s+\S+\s+\S*(.*)$")
    mappings = []
    with open(mapfile, "r") as inp:
        for line in inp:
            match = matcher.match(line)
            if not match:
                # Header lines and such.
                continue
            start = match.group(1)
            end = match.group(2)
            name = match.group(3).strip()
            if name is '' or (name.startswith('[') and name is not '[vdso]'):
                # Skip entries not corresponding to a file.
                continue
            mappings.append((long(start, 16), long(end, 16)))
    return mappings

# A symbol/value pair as expected from gdb frame decorators.


class FrameSymbol(object):
    def __init__(self, sym, val):
        self.sym = sym
        self.val = val

    def symbol(self):
        return self.sym

    def value(self):
        return self.val

# This represents a single JIT frame for the purposes of display.
# That is, the frame filter creates instances of this when it sees a
# JIT frame in the stack.


class JitFrameDecorator(FrameDecorator):
    def __init__(self, base, info, cache):
        super(JitFrameDecorator, self).__init__(base)
        self.info = info
        self.cache = cache

    def _decode_jitframe(self, this_frame):
        calleetoken = long(this_frame['calleeToken_'])
        tag = calleetoken & 3
        calleetoken = calleetoken ^ tag
        function = None
        script = None
        if (tag == self.cache.CalleeToken_Function or
            tag == self.cache.CalleeToken_FunctionConstructing):
            fptr = gdb.Value(calleetoken).cast(self.cache.JSFunction)
            try:
                atom = fptr['atom_']
                if atom:
                    function = str(atom)
            except gdb.MemoryError:
                function = "(could not read function name)"
            script = fptr['u']['i']['s']['script_']
        elif tag == self.cache.CalleeToken_Script:
            script = gdb.Value(calleetoken).cast(self.cache.JSScript)
        return {"function": function, "script": script}

    def function(self):
        if self.info["name"] is None:
            return FrameDecorator.function(self)
        name = self.info["name"]
        result = "<<" + name
        # If we have a frame, we can extract the callee information
        # from it for display here.
        this_frame = self.info["this_frame"]
        if this_frame is not None:
            if gdb.types.has_field(this_frame.type.target(), "calleeToken_"):
                function = self._decode_jitframe(this_frame)["function"]
                if function is not None:
                    result = result + " " + function
        return result + ">>"

    def filename(self):
        this_frame = self.info["this_frame"]
        if this_frame is not None:
            if gdb.types.has_field(this_frame.type.target(), "calleeToken_"):
                script = self._decode_jitframe(this_frame)["script"]
                if script is not None:
                    obj = script['sourceObject_']['value']
                    # Verify that this is a ScriptSource object.
                    # FIXME should also deal with wrappers here.
                    nativeobj = obj.cast(self.cache.NativeObject)
                    # See bug 987069 and despair.  At least this
                    # approach won't give exceptions.
                    class_name = nativeobj['group_']['value']['clasp_']['name'].string(
                        "ISO-8859-1")
                    if class_name != "ScriptSource":
                        return FrameDecorator.filename(self)
                    scriptsourceobj = (
                        nativeobj + 1).cast(self.cache.HeapSlot)[self.cache.SOURCE_SLOT]
                    scriptsource = scriptsourceobj['value']['data']['asBits'] << 1
                    scriptsource = scriptsource.cast(self.cache.ScriptSource)
                    return scriptsource['filename_']['mTuple']['mFirstA'].string()
        return FrameDecorator.filename(self)

    def frame_args(self):
        this_frame = self.info["this_frame"]
        if this_frame is None:
            return FrameDecorator.frame_args(self)
        if not gdb.types.has_field(this_frame.type.target(), "numActualArgs_"):
            return FrameDecorator.frame_args(self)
        # See if this is a function call.
        if self._decode_jitframe(this_frame)["function"] is None:
            return FrameDecorator.frame_args(self)
        # Construct and return an iterable of all the arguments.
        result = []
        num_args = long(this_frame["numActualArgs_"])
        # Sometimes we see very large values here, so truncate it to
        # bypass the damage.
        if num_args > 10:
            num_args = 10
        args_ptr = (this_frame + 1).cast(self.cache.Value.pointer())
        for i in range(num_args + 1):
            # Synthesize names, since there doesn't seem to be
            # anything better to do.
            if i == 0:
                name = 'this'
            else:
                name = 'arg%d' % i
            result.append(FrameSymbol(name, args_ptr[i]))
        return result

# A frame filter for SpiderMonkey.


class SpiderMonkeyFrameFilter(object):
    # |state_holder| is either None, or an instance of
    # SpiderMonkeyUnwinder.  If the latter, then this class will
    # reference the |unwinder_state| attribute to find the current
    # unwinder state.
    def __init__(self, cache, state_holder):
        self.name = "SpiderMonkey"
        self.enabled = True
        self.priority = 100
        self.state_holder = state_holder
        self.cache = cache

    def maybe_wrap_frame(self, frame):
        if self.state_holder is None or self.state_holder.unwinder_state is None:
            return frame
        base = frame.inferior_frame()
        info = self.state_holder.unwinder_state.get_frame(base)
        if info is None:
            return frame
        return JitFrameDecorator(frame, info, self.cache)

    def filter(self, frame_iter):
        return imap(self.maybe_wrap_frame, frame_iter)

# A frame id class, as specified by the gdb unwinder API.


class SpiderMonkeyFrameId(object):
    def __init__(self, sp, pc):
        self.sp = sp
        self.pc = pc

# This holds all the state needed during a given unwind.  Each time a
# new unwind is done, a new instance of this class is created.  It
# keeps track of all the state needed to unwind JIT frames.  Note that
# this class is not directly instantiated.
#
# This is a base class, and must be specialized for each target
# architecture, both because we need to use arch-specific register
# names, and because entry frame unwinding is arch-specific.
# See https://sourceware.org/bugzilla/show_bug.cgi?id=19286 for info
# about the register name issue.
#
# Each subclass must define SP_REGISTER, PC_REGISTER, and
# SENTINEL_REGISTER (see x64UnwinderState for info); and implement
# unwind_entry_frame_registers.


class UnwinderState(object):
    def __init__(self, typecache):
        self.next_sp = None
        self.next_type = None
        self.activation = None
        # An unwinder instance is specific to a thread.  Record the
        # selected thread for later verification.
        self.thread = gdb.selected_thread()
        self.frame_map = {}
        self.proc_mappings = None
        try:
            self.proc_mappings = parse_proc_maps()
        except IOError:
            pass
        self.typecache = typecache

    # If the given gdb.Frame was created by this unwinder, return the
    # corresponding informational dictionary for the frame.
    # Otherwise, return None.  This is used by the frame filter to
    # display extra information about the frame.
    def get_frame(self, frame):
        sp = long(frame.read_register(self.SP_REGISTER))
        if sp in self.frame_map:
            return self.frame_map[sp]
        return None

    # Add information about a frame to the frame map.  This map is
    # queried by |self.get_frame|.  |sp| is the frame's stack pointer,
    # and |name| the frame's type as a string, e.g. "JitFrame_Exit".
    def add_frame(self, sp, name=None, this_frame=None):
        self.frame_map[long(sp)] = {"name": name, "this_frame": this_frame}

    # See whether |pc| is claimed by some text mapping.  See
    # |parse_proc_maps| for details on how the decision is made.
    def text_address_claimed(self, pc):
        for (start, end) in self.proc_mappings:
            if (pc >= start and pc <= end):
                return True
        return False

    # See whether |pc| is claimed by the Jit.
    def is_jit_address(self, pc):
        if self.proc_mappings is not None:
            return not self.text_address_claimed(pc)

        cx = self.get_tls_context()
        runtime = cx['runtime_']['value']
        if long(runtime.address) == 0:
            return False

        jitRuntime = runtime['jitRuntime_']
        if long(jitRuntime.address) == 0:
            return False

        execAllocators = [jitRuntime['execAlloc_'], jitRuntime['backedgeExecAlloc_']]
        for execAlloc in execAllocators:
            for pool in jsjitExecutableAllocator(execAlloc, self.typecache):
                pages = pool['m_allocation']['pages']
                size = pool['m_allocation']['size']
                if pages <= pc and pc < pages + size:
                    return True
        return False

    # Check whether |self| is valid for the selected thread.
    def check(self):
        return gdb.selected_thread() is self.thread

    # Essentially js::TlsContext.get().
    def get_tls_context(self):
        return self.typecache.per_tls_context.value()['mValue']

    # |common| is a pointer to a CommonFrameLayout object.  Return a
    # tuple (local_size, header_size, frame_type), where |size| is the
    # integer size of the previous frame's locals; |header_size| is
    # the size of this frame's header; and |frame_type| is an integer
    # representing the previous frame's type.
    def unpack_descriptor(self, common):
        value = long(common['descriptor_'])
        local_size = value >> self.typecache.FRAMESIZE_SHIFT
        header_size = ((value >> self.typecache.FRAME_HEADER_SIZE_SHIFT) &
                       self.typecache.FRAME_HEADER_SIZE_MASK)
        header_size = header_size * self.typecache.void_starstar.sizeof
        frame_type = long(value & self.typecache.FRAMETYPE_MASK)
        if frame_type == self.typecache.JitFrame_CppToJSJit:
            # Trampoline-x64.cpp pushes a JitFrameLayout object, but
            # the stack pointer is actually adjusted as if a
            # CommonFrameLayout object was pushed.
            header_size = self.typecache.typeCommonFrameLayout.sizeof
        return (local_size, header_size, frame_type)

    # Create a new frame for gdb.  This makes a new unwind info object
    # and fills it in, then returns it.  It also registers any
    # pertinent information with the frame filter for later display.
    #
    # |pc| is the PC from the pending frame
    # |sp| is the stack pointer to use
    # |frame| points to the CommonFrameLayout object
    # |frame_type| is a integer, one of the |enum FrameType| values,
    # describing the current frame.
    # |pending_frame| is the pending frame (see the gdb unwinder
    # documentation).
    def create_frame(self, pc, sp, frame, frame_type, pending_frame):
        # Make a frame_id that claims that |frame| is sort of like a
        # frame pointer for this frame.
        frame_id = SpiderMonkeyFrameId(frame, pc)

        # Read the frame layout object to find the next such object.
        # This lets us unwind the necessary registers for the next
        # frame, and also update our internal state to match.
        common = frame.cast(self.typecache.typeCommonFrameLayoutPointer)
        next_pc = common['returnAddress_']
        (local_size, header_size, next_type) = self.unpack_descriptor(common)
        next_sp = frame + header_size + local_size

        # Compute the type of the next oldest frame's descriptor.
        this_class_type = self.typecache.frame_class_types[frame_type]
        this_frame = frame.cast(this_class_type)

        # Register this frame so the frame filter can find it.  This
        # is registered using SP because we don't have any other good
        # approach -- you can't get the frame id from a gdb.Frame.
        # https://sourceware.org/bugzilla/show_bug.cgi?id=19800
        frame_name = self.typecache.frame_enum_names[frame_type]
        self.add_frame(sp, name=frame_name, this_frame=this_frame)

        # Update internal state for the next unwind.
        self.next_sp = next_sp
        self.next_type = next_type

        unwind_info = pending_frame.create_unwind_info(frame_id)
        unwind_info.add_saved_register(self.PC_REGISTER, next_pc)
        unwind_info.add_saved_register(self.SP_REGISTER, next_sp)
        # FIXME it would be great to unwind any other registers here.
        return unwind_info

    # Unwind an "ordinary" JIT frame.  This is used for JIT frames
    # other than enter and exit frames.  Returns the newly-created
    # unwind info for gdb.
    def unwind_ordinary(self, pc, pending_frame):
        return self.create_frame(pc, self.next_sp, self.next_sp,
                                 self.next_type, pending_frame)

    # Unwind an exit frame.  Returns None if this cannot be done;
    # otherwise returns the newly-created unwind info for gdb.
    def unwind_exit_frame(self, pc, pending_frame):
        if self.activation == 0:
            # Reached the end of the list.
            return None
        elif self.activation is None:
            cx = self.get_tls_context()
            self.activation = cx['jitActivation']
        else:
            self.activation = self.activation['prevJitActivation_']

        packedExitFP = self.activation['packedExitFP_']
        if packedExitFP == 0:
            return None

        exit_sp = pending_frame.read_register(self.SP_REGISTER)
        frame_type = self.typecache.JitFrame_Exit
        return self.create_frame(pc, exit_sp, packedExitFP, frame_type, pending_frame)

    # A wrapper for unwind_entry_frame_registers that handles
    # architecture-independent boilerplate.
    def unwind_entry_frame(self, pc, pending_frame):
        sp = self.next_sp
        # Notify the frame filter.
        self.add_frame(sp, name='JitFrame_CppToJSJit')
        # Make an unwind_info for the per-architecture code to fill in.
        frame_id = SpiderMonkeyFrameId(sp, pc)
        unwind_info = pending_frame.create_unwind_info(frame_id)
        self.unwind_entry_frame_registers(sp, unwind_info)
        self.next_sp = None
        self.next_type = None
        return unwind_info

    # The main entry point that is called to try to unwind a JIT frame
    # of any type.  Returns None if this cannot be done; otherwise
    # returns the newly-created unwind info for gdb.
    def unwind(self, pending_frame):
        pc = pending_frame.read_register(self.PC_REGISTER)

        # If the jit does not claim this address, bail.  GDB defers to our
        # unwinder by default, but we don't really want that kind of power.
        if not self.is_jit_address(long(pc)):
            return None

        if self.next_sp is not None:
            if self.next_type == self.typecache.JitFrame_CppToJSJit:
                return self.unwind_entry_frame(pc, pending_frame)
            return self.unwind_ordinary(pc, pending_frame)
        # Maybe we've found an exit frame.  FIXME I currently don't
        # know how to identify these precisely, so we'll just hope for
        # the time being.
        return self.unwind_exit_frame(pc, pending_frame)

# The UnwinderState subclass for x86-64.


class x64UnwinderState(UnwinderState):
    SP_REGISTER = 'rsp'
    PC_REGISTER = 'rip'

    # A register unique to this architecture, that is also likely to
    # have been saved in any frame.  The best thing to use here is
    # some arch-specific name for PC or SP.
    SENTINEL_REGISTER = 'rip'

    # Must be in sync with Trampoline-x64.cpp:generateEnterJIT.  Note
    # that rip isn't pushed there explicitly, but rather by the
    # previous function's call.
    PUSHED_REGS = ["r15", "r14", "r13", "r12", "rbx", "rbp", "rip"]

    # Fill in the unwound registers for an entry frame.
    def unwind_entry_frame_registers(self, sp, unwind_info):
        sp = sp.cast(self.typecache.void_starstar)
        # Skip the "result" push.
        sp = sp + 1
        for reg in self.PUSHED_REGS:
            data = sp.dereference()
            sp = sp + 1
            unwind_info.add_saved_register(reg, data)
            if reg is "rbp":
                unwind_info.add_saved_register(self.SP_REGISTER, sp)

# The unwinder object.  This provides the "user interface" to the JIT
# unwinder, and also handles constructing or destroying UnwinderState
# objects as needed.


class SpiderMonkeyUnwinder(Unwinder):
    # A list of all the possible unwinders.  See |self.make_unwinder|.
    UNWINDERS = [x64UnwinderState]

    def __init__(self, typecache):
        super(SpiderMonkeyUnwinder, self).__init__("SpiderMonkey")
        self.typecache = typecache
        self.unwinder_state = None

        # Disabled by default until we figure out issues in gdb.
        self.enabled = False
        gdb.write("SpiderMonkey unwinder is disabled by default, to enable it type:\n" +
                  "\tenable unwinder .* SpiderMonkey\n")
        # Some versions of gdb did not flush the internal frame cache
        # when enabling or disabling an unwinder.  This was fixed in
        # the same release of gdb that added the breakpoint_created
        # event.
        if not hasattr(gdb.events, "breakpoint_created"):
            gdb.write("\tflushregs\n")

        # We need to invalidate the unwinder state whenever the
        # inferior starts executing.  This avoids having a stale
        # cache.
        gdb.events.cont.connect(self.invalidate_unwinder_state)
        assert self.test_sentinels()

    def test_sentinels(self):
        # Self-check.
        regs = {}
        for unwinder in self.UNWINDERS:
            if unwinder.SENTINEL_REGISTER in regs:
                return False
            regs[unwinder.SENTINEL_REGISTER] = 1
        return True

    def make_unwinder(self, pending_frame):
        # gdb doesn't provide a good way to find the architecture.
        # See https://sourceware.org/bugzilla/show_bug.cgi?id=19399
        # So, we look at each known architecture and see if the
        # corresponding "unique register" is known.
        for unwinder in self.UNWINDERS:
            try:
                pending_frame.read_register(unwinder.SENTINEL_REGISTER)
            except Exception:
                # Failed to read the register, so let's keep going.
                # This is more fragile than it might seem, because it
                # fails if the sentinel register wasn't saved in the
                # previous frame.
                continue
            return unwinder(self.typecache)
        return None

    def __call__(self, pending_frame):
        if self.unwinder_state is None or not self.unwinder_state.check():
            self.unwinder_state = self.make_unwinder(pending_frame)
        if not self.unwinder_state:
            return None
        return self.unwinder_state.unwind(pending_frame)

    def invalidate_unwinder_state(self, *args, **kwargs):
        self.unwinder_state = None

# Register the unwinder and frame filter with |objfile|.  If |objfile|
# is None, register them globally.


def register_unwinder(objfile):
    type_cache = UnwinderTypeCache()
    unwinder = None
    # This currently only works on Linux, due to parse_proc_maps.
    if _have_unwinder and platform.system() == "Linux":
        unwinder = SpiderMonkeyUnwinder(type_cache)
        gdb.unwinder.register_unwinder(objfile, unwinder, replace=True)
    # We unconditionally register the frame filter, because at some
    # point we'll add interpreter frame filtering.
    filt = SpiderMonkeyFrameFilter(type_cache, unwinder)
    if objfile is None:
        objfile = gdb
    objfile.frame_filters[filt.name] = filt
