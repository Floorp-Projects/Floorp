# Hacking Tips

**These tips were archived from [MDN](https://mdn-archive.mossop.dev/en-US/docs/Mozilla/Projects/SpiderMonkey/Hacking_Tips)**: This may be out of date!

This is archived here because it captures valuable documentation that even if potentially out of date, provides inspiration.

---


This page lists a few tips to help you investigate issues related to SpiderMonkey. All tips listed here are dealing with the JavaScript shell obtained at the end of the [build documentation of SpiderMonkey](build.rst). It is separated in 2 parts, one section related to debugging and another section related to drafting optimizations. Many of these tips only apply to debug builds of the JS shell; they will not function in a release build.

## Tools

Here are some debugging tools above and beyond your standard debugger that might help you:

* [rr](https://rr-project.org/) is a record-and-replay deterministic debugger for Linux
* [Pernosco](https://pernos.co/) takes an rr recording and adds omniscient debugging tools to help you
  * It is a paid service with a free trial
  * Mozilla has a license for internal developers; you can contact Matthew Gaudet <mgaudet@mozilla.com> for details


## Debugging Tips

### Getting help (from JS shell)

Use the **help** function to get the list of all primitive functions of the shell with their description. Note that some functions have been moved under an 'os' object, and **help(os)** will give brief help on just the members of that "namespace".

You can also use **help(/Regex/)** to get help for members of the global namespace that match the given regular expression.

### Getting the bytecode of a function (from JS shell)

The shell has a small function named **dis** to dump the bytecode of a function with its source notes.  Without arguments, it will dump the bytecode of its caller.

```
js> function f () {
  return 1;
}
js> dis(f);
flags:
loc     op
-----   --
main:
00000:  one
00001:  return
00002:  stop

Source notes:
 ofs  line    pc  delta desc     args
---- ---- ----- ------ -------- ------
  0:    1     0 [   0] newline
  1:    2     0 [   0] colspan 2
  3:    2     2 [   2] colspan 9

```

### Getting the bytecode of a function (from gdb)

In _jsopcode.cpp_, a function named **js::DisassembleAtPC** can print the bytecode of a script.  Some variants of this function, such as **js::DumpScript** etc., are convenient for debugging.

### Printing the JS stack (from gdb)

In _jsobj.cpp_, a function named **js::DumpBacktrace** prints a backtrace à la gdb for the JS stack.  The backtrace contains in the following order, the stack depth, the interpreter frame pointer (see _js/src/vm/Stack.h_, **StackFrame** class) or (nil) if compiled with IonMonkey, the file and line number of the call location and under parentheses, the **JSScript** pointer and the **jsbytecode** pointer (pc) executed.

```
$ gdb --args js
[…]
(gdb) b js::ReportOverRecursed
(gdb) r
js> function f(i) {
  if (i % 2) f(i + 1);
  else f(i + 3);
}
js> f(0)

Breakpoint 1, js::ReportOverRecursed (maybecx=0xfdca70) at /home/nicolas/mozilla/ionmonkey/js/src/jscntxt.cpp:495
495         if (maybecx)
(gdb) call js::DumpBacktrace(maybecx)
#0          (nil)   typein:2 (0x7fffef1231c0 @ 0)
#1          (nil)   typein:2 (0x7fffef1231c0 @ 24)
#2          (nil)   typein:3 (0x7fffef1231c0 @ 47)
#3          (nil)   typein:2 (0x7fffef1231c0 @ 24)
#4          (nil)   typein:3 (0x7fffef1231c0 @ 47)
[…]
#25157 0x7fffefbbc250   typein:2 (0x7fffef1231c0 @ 24)
#25158 0x7fffefbbc1c8   typein:3 (0x7fffef1231c0 @ 47)
#25159 0x7fffefbbc140   typein:2 (0x7fffef1231c0 @ 24)
#25160 0x7fffefbbc0b8   typein:3 (0x7fffef1231c0 @ 47)
#25161 0x7fffefbbc030   typein:5 (0x7fffef123280 @ 9)

```

Note, you can do the exact same exercise above using `lldb` (necessary on OSX after Apple removed `gdb`) by running `lldb -f js` then following the remaining steps.

Since SpiderMonkey 48, we have a gdb unwinder.  This unwinder is able to read the frames created by the JIT, and to display the frames which are after these JIT frames.

```
$ gdb --args out/dist/bin/js ./foo.js
[…]
SpiderMonkey unwinder is disabled by default, to enable it type:
        enable unwinder .* SpiderMonkey
(gdb) b js::math_cos
(gdb) run
[…]
#0  js::math_cos (cx=0x14f2640, argc=1, vp=0x7fffffff6a88) at js/src/jsmath.cpp:338
338         CallArgs args = CallArgsFromVp(argc, vp);
(gdb) enable unwinder .* SpiderMonkey
(gdb) backtrace 10
#0  0x0000000000f89979 in js::math_cos(JSContext*, unsigned int, JS::Value*) (cx=0x14f2640, argc=1, vp=0x7fffffff6a88) at js/src/jsmath.cpp:338
#1  0x0000000000ca9c6e in js::CallJSNative(JSContext*, bool (*)(JSContext*, unsigned int, JS::Value*), JS::CallArgs const&) (cx=0x14f2640, native=0xf89960 , args=...) at js/src/jscntxtinlines.h:235
#2  0x0000000000c87625 in js::Invoke(JSContext*, JS::CallArgs const&, js::MaybeConstruct) (cx=0x14f2640, args=..., construct=js::NO_CONSTRUCT) at js/src/vm/Interpreter.cpp:476
#3  0x000000000069bdcf in js::jit::DoCallFallback(JSContext*, js::jit::BaselineFrame*, js::jit::ICCall_Fallback*, uint32_t, JS::Value*, JS::MutableHandleValue) (cx=0x14f2640, frame=0x7fffffff6ad8, stub_=0x1798838, argc=1, vp=0x7fffffff6a88, res=JSVAL_VOID) at js/src/jit/BaselineIC.cpp:6113
#4  0x00007ffff7f41395 in
```

Note, when you enable the unwinder, the current version of gdb (7.10.1) does not flush the backtrace. Therefore, the JIT frames do not appear until you settle on the next breakpoint. To work-around this issue you can use the recording feature of `gdb`, to step one instruction, and settle back to where you came from with the following set of `gdb` commands:

```
(gdb) record full
(gdb) si
(gdb) record goto 0
(gdb) record stop
```

If you have a core file, you can use the gdb unwinder the same way, or do everything from the command line as follows:

```
$ gdb -ex 'enable unwinder .* SpiderMonkey' -ex 'bt 0' -ex 'thread apply all backtrace' -ex 'quit' out/dist/bin/js corefile
```

The gdb unwinder is supposed to be loaded by `dist/bin/js-gdb.py` and load python scripts which are located in `js/src/gdb/mozilla` under gdb. If gdb does not load the unwinder by default, you can force it to, by using the `source` command with the `js-gdb.py` file.

### Setting a breakpoint in the generated code (from gdb, x86 / x86-64, arm)

To set a breakpoint in the generated code of a specific JSScript compiled with IonMonkey, set a breakpoint on the instruction you are interested in. If you have no precise idea which function you are looking at, you can set a breakpoint on the **js::ion::CodeGenerator::visitStart** function.  Optionally, a condition on the **ins->id()** of the LIR instruction can be added to select precisely the instruction you are looking for. Once the breakpoint is on the **CodeGenerator** function of the LIR instruction, add a command to generate a static breakpoint in the generated code.

```
$ gdb --args js
[…]
(gdb) b js::ion::CodeGenerator::visitStart
(gdb) command
>call masm.breakpoint()
>continue
>end
(gdb) r
js> function f(a, b) { return a + b; }
js> for (var  i = 0; i < 100000; i++) f(i, i + 1);

Breakpoint 1, js::ion::CodeGenerator::visitStart (this=0x101ed20, lir=0x10234e0)
    at /home/nicolas/mozilla/ionmonkey/js/src/ion/CodeGenerator.cpp:609
609     }

Program received signal SIGTRAP, Trace/breakpoint trap.
0x00007ffff7fb165a in ?? ()
(gdb)

```

Once you hit the generated breakpoint, you can replace it by a gdb breakpoint to make it conditional. The procedure is to first replace the generated breakpoint by a nop instruction, and to set a breakpoint at the address of the nop.

```
(gdb) x /5i $pc - 1
   0x7ffff7fb1659:      int3
=> 0x7ffff7fb165a:      mov    0x28(%rsp),%rax
   0x7ffff7fb165f:      mov    %eax,%ecx
   0x7ffff7fb1661:      mov    0x30(%rsp),%rdx
   0x7ffff7fb1666:      mov    %edx,%ebx

(gdb) # replace the int3 by a nop
(gdb) set *(unsigned char *) ($pc - 1) = 0x90
(gdb) x /1i $pc - 1
   0x7ffff7fb1659:      nop

(gdb) # set a breakpoint at the previous location
(gdb) b *0x7ffff7fb1659
Breakpoint 2 at 0x7ffff7fb1659
```

### Printing Ion generated assembly code (from gdb)

If you want to look at the assembly code generated by IonMonkey, you can follow this procedure:

1. Place a breakpoint at CodeGenerator.cpp on the CodeGenerator::link method.
1. Step next a few times, so that the "code" variable gets generated
1. Print code->code\_, which is the address of the code
1. Disassemble code read at this address (using x/Ni address, where N is the number of instructions you would like to see)

Here is an example. It might be simpler to use the CodeGenerator::link lineno instead of the full qualified name to put the breakpoint. Let's say that the line number of this function is 4780, for instance:

    (gdb) b CodeGenerator.cpp:4780
    Breakpoint 1 at 0x84cade0: file /home/code/mozilla-central/js/src/ion/CodeGenerator.cpp, line 4780.
    (gdb) r
    Starting program: /home/code/mozilla-central/js/src/32-release/js -f /home/code/jaeger.js
    [Thread debugging using libthread_db enabled]
    Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
    [New Thread 0xf7903b40 (LWP 12563)]
    [New Thread 0xf6bdeb40 (LWP 12564)]
    Run#0

    Breakpoint 1, js::ion::CodeGenerator::link (this=0x86badf8)
        at /home/code/mozilla-central/js/src/ion/CodeGenerator.cpp:4780
    4780    {
    (gdb) n
    4781        JSContext *cx = GetIonContext()->cx;
    (gdb) n
    4783        Linker linker(masm);
    (gdb) n
    4784        IonCode *code = linker.newCode(cx, JSC::ION*CODE);
    (gdb) n
    4785        if (!code)
    (gdb) p code->code*
    $1 = (uint8_t \*) 0xf7fd25a8 "\201", <incomplete sequence \354\200>
    (gdb) x/2i 0xf7fd25a8
       0xf7fd25a8:    sub    $0x80,%esp
       0xf7fd25ae:    mov    0x94(%esp),%ecx

On arm, the compiled JS code will always be ARM machine code, whereas SpiderMonkey itself is frequently Thumb2.  Since there isn't debug info for the JIT'd code, you will need to tell gdb that you are looking at ARM code:

    (gdb) set arm force-mode arm

Or you can wrap the x command in your own command:

    def xi
        set arm force-mode arm
        eval "x/%di %d", $arg0, $arg1
        set arm force-mode auto
    end

### Printing asm.js/wasm generated assembly code (from gdb)

- Set a breakpoint on `js::wasm::Instance::callExport` (defined in `WasmInstance.cpp` as of November 18th 2016). This will trigger for _any_ asm.js/wasm call, so you should find a way to set this breakpoint for the only generated codes you want to look at.
- Run the program.
- Do `next` in gdb until you reach the definition of the `funcPtr`:
```
// Call the per-exported-function trampoline created by GenerateEntry.
auto funcPtr = JS*DATA_TO_FUNC_PTR(ExportFuncPtr, codeBase() + func.entryOffset());
if (!CALL_GENERATED_2(funcPtr, exportArgs.begin(), &tlsData*))
    return false;
```
- After it's set, `x/64i funcPtr` will show you the trampoline code. There should be a call to an address at some point; that's what we're targeting. Copy that address.

```
   0x7ffff7ff6000:    push   %r15
   0x7ffff7ff6002:    push   %r14
   0x7ffff7ff6004:    push   %r13
   0x7ffff7ff6006:    push   %r12
   0x7ffff7ff6008:    push   %rbp
   0x7ffff7ff6009:    push   %rbx
   0x7ffff7ff600a:    movabs $0xea4f80,%r10
   0x7ffff7ff6014:    mov    0x178(%r10),%r10
   0x7ffff7ff601b:    mov    %rsp,0x40(%r10)
   0x7ffff7ff601f:    mov    (%rsi),%r15
   0x7ffff7ff6022:    mov    %rdi,%r10
   0x7ffff7ff6025:    push   %r10
   0x7ffff7ff6027:    test   $0xf,%spl
   0x7ffff7ff602b:    je     0x7ffff7ff6032
   0x7ffff7ff6031:    int3
   0x7ffff7ff6032:    callq  0x7ffff7ff5000     <------ right here
```

- `x/64i address` (in this case: `x/64i 0x7ffff7ff6032`).
- If you want to put a breakpoint at the function's entry, you can do: `b *address` (for instance here,  `b* 0x7ffff7ff6032`). Then you can display the instructions around pc with `x/20i $pc,` and execute instruction by instruction with `stepi`.

### Finding the script of Ion generated assembly (from gdb)

When facing a bug in which you are in the middle of IonMonkey generated code, the first thing to note is that gdb's backtrace is not reliable, because the generated code does not keep a frame pointer. To figure it out, you have to read the stack to infer the IonMonkey frame.

```
(gdb) x /64a $sp
[…]
0x7fffffff9838: 0x7ffff7fad2da  0x141
0x7fffffff9848: 0x7fffef134d40  0x2
[…]
(gdb) p (*(JSFunction**) 0x7fffffff9848)->u.i.script_->lineno
$1 = 1
(gdb) p (*(JSFunction**) 0x7fffffff9848)->u.i.script_->filename
$2 = 0xff92d1 "typein"
```

The stack is ordered as defined in js/src/ion/IonFrames-x86-shared.h. It is composed of the return address, a descriptor (a small value), the JSFunction (if it is even) or a JSScript (if it is odd; remove it to dereference the pointer) and the frame ends with the number of actual arguments (a small value too). If you want to know at which LIR the code is failing at, the **js::ion::CodeGenerator::generateBody** function can be instrumented to dump the LIR **id** before each instruction.

```
for (; iter != current->end(); iter++) {
    IonSpew(IonSpew_Codegen, "instruction %s", iter->opName());
    […]

    masm.store16(Imm32(iter->id()), Address(StackPointer, -8)); // added
    if (!iter->accept(this))
        return false;
```

`This modification will add an instruction which abuses the stack pointer` to store an immediate value (the LIR id) to a location which would never be generated by any sane compiler. Thus when dumping the assembly under gdb, this kind of instructions would be easily noticeable.

### Viewing the MIRGraph of Ion/Odin compilations (from gdb)

With gdb instrumentation, we can call [iongraph](https://github.com/sstangl/iongraph) program within gdb when the execution is stopped.  This instrumentation adds an **`iongraph`** command when provided with an instance of a **`MIRGenerator*`**, will call `iongraph`, `graphviz` and your preferred png viewer to display the MIR graph at the precise time of the execution.  To find **`MIRGenetator*`** instances, it is best to look up into the stack for `OptimizeMIR`, or `CodeGenerator::generateBody`.  **`OptimizeMIR`** function has a **`mir`** argument, and the **`CodeGenerator::generateBody`** function has a member **`this->gen`**.

```
(gdb) bt
#0  0x00000000007eaad4 in js::InlineList<js::jit::MBasicBlock>::begin() const (this=0x33dbbc0) at …/js/src/jit/InlineList.h:280
#1  0x00000000007cb845 in js::jit::MIRGraph::begin() (this=0x33dbbc0) at …/js/src/jit/MIRGraph.h:787
#2  0x0000000000837d25 in js::jit::BuildPhiReverseMapping(js::jit::MIRGraph&) (graph=...) at …/js/src/jit/IonAnalysis.cpp:2436
#3  0x000000000083317f in js::jit::OptimizeMIR(js::jit::MIRGenerator*) (mir=0x33dbdf0) at …/js/src/jit/Ion.cpp:1570
…
(gdb) frame 3
#3  0x000000000083317f in js::jit::OptimizeMIR(js::jit::MIRGenerator*) (mir=0x33dbdf0) at …/js/src/jit/Ion.cpp:1570
(gdb) iongraph mir
 function 0 (asm.js compilation): success; 1 passes.
/* open your png viewer with the result of iongraph */
```

This gdb instrumentation is supposed to work with debug builds, or with optimized builds compiled with `--enable-jitspew` configure flag. External programs such as `iongraph`, `dot`, and your png viewer are searched for in the `PATH`; otherwise custom one can either be configured with environment variables (`GDB_IONGRAPH`, `GDB_DOT`, `GDB_PNGVIEWER`) before starting gdb, or with gdb parameters  (`set iongraph-bin <path>`, `set dot-bin <path>`, `set pngviewer-bin <path>`) within gdb.

Enabling GDB instrumentation may require launching a JS shell executable that shares a directory with a file name "js-gdb.py". If js/src/js does not provide the "iongraph" command, try js/src/shell/js. GDB may complain that ~/.gdbinit requires modification to authorize user scripts, and if so will print out directions.

### Finding the code that generated a JIT instruction (from rr)

If you are looking at a JIT instruction and need to know what code generated it, you can use [jitsrc.py](https://searchfox.org/mozilla-central/source/js/src/gdb/mozilla/jitsrc.py). This script adds a `jitsrc` command to rr that will trace backwards from the JIT instruction to the code that generated it.

To use the `jitsrc` command, add the following line to your .gdbinit file, or run it manually:

    source js/src/gdb/mozilla/jitsrc.py

And you use the command like this: `jitsrc <address of JIT instruction>`.

Running the command will leave the application at the point of execution where that JIT instruction was originally emitted. For example, the backtrace might contain a frame at [js::jit::MacroAssemblerX64::loadPtr](https://searchfox.org/mozilla-central/rev/ddde3bbcafabe0fc8a36c660b3b673507d3e3874/js/src/jit/x64/MacroAssembler-x64.h#575).

The way this works is by setting a watchpoint on the JIT instruction and `reverse-continue`ing the program execution to reach the point when that memory address was assigned to. JIT instruction memory can be copied or moved, so the `jitsrc` command automates updating the watchpoint across the copy/move to continue back to the original source of the JIT instruction.

### Break on valgrind errors

Sometimes, a bug can be reproduced under valgrind but with great difficulty under gdb.  One way to investigate is to let valgrind start gdb for you; the other way documented here is to let valgrind act as a gdb server which can be manipulated from the gdb remote.

```
$ valgrind --smc-check=all-non-file
```

This command will tell you how to start gdb as a remote. Be aware that functions which are usually dumping some output will do it in the shell where valgrind is started and not in the shell where gdb is started. Thus functions such as **js::DumpBacktrace**, when called from gdb, will print their output in the shell containing valgrind.

### Adding spew for Compilations & Bailouts & Invalidations (from gdb)

If you are in rr, and forgot to record with the spew enabled with IONFLAGS or because this is an optimized build, then you can add similar spew with extra breakpoints within gdb.  gdb has the ability to set breakpoints with commands, but a simpler / friendlier version is to use **dprintf**, with a location, and followed by printf-like arguments.

    (gdb) dprintf js::jit::IonBuilder::IonBuilder, "Compiling %s:%d:%d-%d\n", info->script*->scriptSource()->filename*.mTuple.mFirstA, info->script*->lineno*, info->script*->sourceStart*, info->script*->sourceEnd*
    Dprintf 1 at 0x7fb4f6a104eb: file /home/nicolas/mozilla/contrib-push/js/src/jit/IonBuilder.cpp, line 159.
    (gdb) cond 1 inliningDepth == 0
    (gdb) dprintf js::jit::BailoutIonToBaseline, "Bailout from %s:%d:%d-%d\n", iter.script()->scriptSource()->filename*.mTuple.mFirstA, iter.script()->lineno*, iter.script()->sourceStart*, iter.script()->sourceEnd*
    Dprintf 2 at 0x7fb4f6fe43dc: js::jit::BailoutIonToBaseline. (2 locations)
    (gdb) dprintf Ion.cpp:3196, "Invalidate %s:%d:%d-%d\n", co->script*->scriptSource()->filename*.mTuple.mFirstA, co->script*->lineno*, co->script*->sourceStart*, co->script*->sourceEnd*
    Dprintf 3 at 0x7fb4f6a0b62a: file /home/nicolas/mozilla/contrib-push/js/src/jit/Ion.cpp, line 3196.
    `(gdb) continue`
    Compiling self-hosted:650:20470-21501
    Bailout from self-hosted:20:403-500
    Invalidate self-hosted:20:403-500

Note: the line 3196, listed above, corresponds to the location of the [Jit spew inside jit::Invalidate function](https://searchfox.org/mozilla-central/rev/655f49c541108e3d0a232aa7173fbcb9af88d80b/js/src/jit/Ion.cpp#2475).

## Hacking tips


### Using the Gecko Profiler (browser / xpcshell)

See the section dedicated to [profiling with the Gecko Profiler](/tools/profiler/index.rst). This method of profiling has the advantage of mixing the JavaScript stack with the C++ stack, which is useful for analyzing library function issues.

One tip is to start looking at a script with an inverted JS stack to locate the most expensive JS function, then to focus on the frame of this JS function, and to remove the inverted stack and look at C++ part of this function to determine from where the cost is coming from.

These archived [tips on using the Gecko Profiler](https://mdn-archive.mossop.dev/en-US/docs/Performance/Profiling_with_the_Built-in_Profiler "/en-US/docs/Performance/Profiling_with_the_Built-in_Profiler") and [FAQ](https://mdn-archive.mossop.dev/en-US/docs/Mozilla/Performance/Gecko_Profiler_FAQ "Gecko Profiler FAQ") might also be useful as inspiration, but are old enough that they are probably not accurate any more.

### Using callgrind (JS shell)

Because SpiderMonkey just-in-time compilers rewrite the executed program, valgrind should be informed from the command line by adding **--smc-check=all-non-file**.

```
$ valgrind --tool=callgrind --callgrind-out-file=bench.clg \
     --smc-check=all-non-file
```

The output file can then be used with **kcachegrind**, which provides a graphical view of the call graph.

### Using IonMonkey spew (JS shell)

IonMonkey spew is extremely verbose (not as much as the INFER spew), but you can filter it to focus on the list of compiled scripts or channels, IonMonkey spew channels can be selected with the IONFLAGS environment variable, and compilation spew can be filtered with IONFILTER.

IONFLAGS contains the names of [each channel separated by commas](https://searchfox.org/mozilla-central/source/js/src/jit/JitSpewer.cpp#338). The **logs** channel produces one file (_/tmp/ion.json_), made to be used with [iongraph](https://github.com/sstangl/iongraph) (made by Sean Stangl). This tool will show the MIR & LIR steps done by IonMonkey during the compilation. To use [iongraph](https://github.com/sstangl/iongraph), you must install [Graphviz](https://www.graphviz.org/download/ "graphviz downloads").

Compilation logs and spew can be filtered with the IONFILTER environment variable which contains locations as output by other spew channels. Multiple locations can be specified using comma as a separator.

```
$ IONFILTER=pdfjs.js:16934 IONFLAGS=logs,scripts,osi,bailouts ./js --ion-offthread-compile=off ./run.js 2>&1 | less
```

The **bailouts** channel is likely to be the first thing you should focus on, because this means that something does not stay in IonMonkey and fallback to the interpreter. This channel outputs locations (as returned by the **id()** function of both instructions) of the latest MIR and the latest LIR phases. These locations should correspond to phases of the **logs** and a filter can be used to remove uninteresting functions.

### Using the ARM simulator

The ARM simulator can be used to test the ARM JIT backend on x86/x64 hardware. An ARM simulator build is an x86 shell (or browser) with the ARM JIT backend. Instead of entering JIT code, it runs it in a simulator (interpreter) for ARM code. To use the simulator, compile an x86 shell (32-bit, x64 doesn't work as we use a different Value format there), and pass --enable-arm-simulator to configure. For instance, on a 64-bit Linux host you can use the following configure command to get an ARM simulator build:

```shell
AR=ar CC="gcc -m32" CXX="g++ -m32" ../configure --target=i686-pc-linux
--enable-debug --disable-optimize --enable-threadsafe --enable-simulator=arm
```

Or on OS X:

```shell
$ AR=ar CC="clang -m32" CXX="clang++ -m32" ../configure --target=i686-apple-darwin10.0.0 --enable-debug --disable-optimize --enable-threadsafe --enable-arm-simulator
```

An **--enable-debug --enable-optimize** build is recommended if you want to run jit-tests or jstests.

#### Use the VIXL Debugger in the simulator (arm64)

Set a breakpoint (see the section above about setting a breakpoint in generated code) and run with the environment variable `USE_DEBUGGER=1`. This will then drop you into a simple debugger provided with VIXL, the ARM simulator technology used for arm64 simulation.

#### Use the Simulator Debugger for arm32

The same instructions for arm64 in the preceding section apply, but the environment variable differs: Use `ARM_SIM_DEBUGGER=1`.

#### Building the browser with the ARM simulator

You can also build the entire browser with the ARM simulator backend, for instance to reproduce browser-only JS failures on ARM. Make sure to build a browser for x86 (32-bits) and add this option to your mozconfig file:

ac_add_options --enable-arm-simulator

If you are under an Ubuntu or Debian 64-bits distribution and you want to build a 32-bits browser, it might be hard to find the relevant 32-bits dependencies. You can use [padenot's scripts](https://github.com/padenot/fx-32-on-64.sh) which will magically setup a chrooted 32-bits environment and do All The Things (c) for you (you just need to modify the mozconfig file).

### Using rr on a test

Get the command line for your test run using -s:

./jit_test.py -s $JS_SHELL saved-stacks/async.js

Insert 'rr' before the shell invocation:

```
rr $JS_SHELL -f $JS_SRC/jit-test/lib/prolog.js --js-cache $JS_SRC/jit-test/.js-cache -e "const platform='linux2'; const libdir='$JS_SRC/jit-test/lib/'; const scriptdir='$JS_SRC/jit-test/tests/saved-stacks/'" -f $JS_SRC/jit-test/tests/saved-stacks/async.js
```

(note that the above is an example; simply setting JS_SHELL and JS_SRC will not work). Or if this is an intermittent, run it in a loop capturing an rr log for every one until it fails:

```
n=1; while rr ...same.as.above...; do echo passed $n; n=$(( $n + 1 )); done
```

Wait until it hits a failure. Now you can run `rr replay` to replay that last (failed) run under gdb.

#### rr with reftest

To break on the write of a differing pixel:

1. Find the X/Y of a pixel that differs
2. Use `run Z` where Z is the mark in the log for TEST-START. For example in '[rr 28496 607198]REFTEST TEST-START | file:///home/bgirard/mozilla-central/tree/image/test/reftest/bmp/bmpsuite/b/wrapper.html?badpalettesize.bmp', Z would be 607198.
3. `break 'mozilla::dom::CanvasRenderingContext2D::DrawWindow(nsGlobalWindow&, double, double, double, double, nsAString_internal const&, unsigned int, mozilla::ErrorResult&)'`
4. `cont`
5. `break 'PresShell::RenderDocument(nsRect const&, unsigned int, unsigned int, gfxContext\*)'`
6. `set print object on`
7. `set $x = <YOUR X VALUE>`
8. `set $y = <YOUR Y VALUE>`
9. `print &((cairo_image_surface_t*)aThebesContext->mDT.mRawPtr->mSurface).data[$y * ((cairo_image_surface_t*)aThebesContext->mDT.mRawPtr->mSurface).stride + $x * ((cairo_image_surface_t\*)aThebesContext->mDT.mRawPtr->mSurface).depth / 8]`
10. `watch *(char*)<ADDRESS OF PREVIOUS COMMAND>` (NOTE: If you set a watch on the previous expression gdb will watch the expression and run out of watchpoints)

#### rr with emacs

Within emacs, do `M-x gud-gdb` and replace the command line with `rr replay`. When gdb comes up, enter

```
set annot 1
```

to get it to emit file location information so that emacs will pop up the corresponding source. Note that if you `reverse-continue` over a SIGSEGV and you're using the standard .gdbinit that sets a catchpoint for that signal, you'll get an additional stop at the catchpoint. Just `reverse-continue` again to continue to your breakpoints or whatever.

### [Hack] Replacing one instruction

To replace one specific instruction, you can customize the instruction's visit function using the JSScript **filename** in **lineno** fields, as well as the **id()** of the LIR / MIR instructions.  The JSScript can be obtained from **info().script()**.

```
bool
CodeGeneratorX86Shared::visitGuardShape(LGuardShape *guard)
{
    if (info().script()->lineno == 16934 && guard->id() == 522) {
        [… another impl only for this one …]
        return true;
    }
    [… old impl …]
```

### [Hack] Spewing all compiled code

I usually just add this to the appropriate `executableCopy()` function.

    if (getenv("INST_DUMP")) {
        char buf[4096];
        sprintf(buf, "gdb /proc/%d/exe %d -batch -ex 'set pagination off' -ex 'set arm force-mode arm' -ex 'x/%di %p' -ex 'set arm force-mode auto'", getpid(), getpid(), m_buffer.size() / 4, buffer);
        system(buf);
    }

If you aren't running on arm, you should omit the `-ex 'set arm force-mode arm'` and `-ex 'set arm force-mode auto'`.  And you should change the size()/4 to be something more appropriate for your architecture.

### Benchmarking with sub-milliseconds (JS shell)

In the shell, we have 2 simple ways to benchmark a script. We can either use the **-b** shell option (**--print-timing**) which will evaluate a script given on the command line without any need to instrument the benchmark and print an extra line showing the run-time of the script.  The other way is to wrap the section that you want to measure with the **dateNow()** function call, which returns the number of milliseconds, with a decimal part for sub-milliseconds.

```js
js> dateNow() - dateNow()
-0.0009765625
```

Since [Firefox 61](https://bugzilla.mozilla.org/show_bug.cgi?id=1439788), the shell also has **performance.now()** available.

### Benchmarking with sub-milliseconds (browser)

Similar to how you can use **dateNow()** in the JS shell, you can use **performance.now()** in the JavaScript code of a page.

### Dumping the JavaScript heap

From the shell, you can call the `dumpHeap` function to dump out all GC things (reachable and unreachable) that are present in the heap. By default, the function writes to stdout, but a filename can be specified as an argument.

Example output might look as follows:

```
0x1234abcd B global object
```

The output is textual. The first section of the file contains a list of roots, one per line. Each root has the form "0xabcd1234 \<color> \<description>", where \<color> is the color of the given GC thing (B for black, G for gray, W for white) and \<description> is a string. The list of roots ends with a line containing "==========".

After the roots come a series of zones. A zone starts with several "comment lines" that start with hashes. The first comment declares the zone. It is followed by lines listing each compartment within the zone. After all the compartments come arenas, which is where the GC things are actually stored. Each arena is followed by all the GC things in the arena. A GC thing starts with a line giving its address, its color, and the thing kind (object, function, whatever). After this comes a list of addresses that the GC thing points to, each one starting with ">".

It's also possible to dump the JavaScript heap from C++ code (or from gdb) using the `js::DumpHeap` function. It is part of jsfriendapi.h and it is available in release builds.

### Inspecting MIR objects within a debugger

For MIRGraph, MBasicBlock, and MDefinition and its subclasses (MInstruction, MConstant, etc.), call the dump member function.

    (gdb) call graph->dump()
    (gdb) call block->dump()
    (gdb) call def->dump()


### How to debug oomTest() failures

The oomTest() function executes a piece of code many times, simulating an OOM failure at each successive allocation it makes.  It's designed to highlight incorrect OOM handling, which may show up as a crash or assertion failure at some later point.

When debugging such a crash, the most useful thing is to locate the last simulated allocation failure, as that is usually what has caused the subsequent crash.

My workflow for doing this is as follows:

1. Build a version of the engine with `--enable-debug` and `--enable-oom-breakpoint` configure flags.
2. Set the environment variable `OOM_VERBOSE=1` and reproduce the failure.  This will print an allocation count at each simulated failure.  Note the count of the last allocation.
3. Run the engine under a debugger and set a breakpoint on the function `js_failedAllocBreakpoint`.
4. Run the program and `continue` the necessary number of times until you reach the final allocation.
   - e.g. in lldb, if the allocation failure number shown is 1500, run `continue -i 1498` (subtracted 2 because we've already hit it once and don't want to skip the last). Drop "-i" for gdb.
5. Dump a backtrace.  This should show you the point at which the OOM is incorrectly handled, which will be a few frames up from the breakpoint.

Note: if you are on linux, it may be simpler to use rr.

Some guidelines for handling OOM that lead to failures when they are not followed:

1. Check for allocation failure!
   - Fallible allocations should always must be checked and handled, at a minimum by returning a status indicating failure to the caller.
2. Report OOM to the context if you have one
   - If a function has a `JSContext*` argument, usually it should call `js::ReportOutOfMemory(cx)` on allocation failure to report this to the context.
3. Sometimes it's OK to ignore OOM
   - For example if you are performing a speculative optimisation you might abandon it and continue anyway.  In this case, you may have to call cx->recoverFromOutOfMemory() if something further down the stack has already reported the failure.

### Debugging GC marking/rooting

The **js::debug** namespace contains some functions that are useful for watching mark bits for an individual JSObject* (or any Cell*). [js/src/gc/Heap.h](https://searchfox.org/mozilla-central/rev/dc5027f02e5ea1d6b56cfbd10f4d3a0830762115/js/src/gc/Heap.h#817-835) contains a comment describing an example usage. Reproduced here:

    // Sample usage from gdb:
    //
    //   (gdb) p $word = js::debug::GetMarkWordAddress(obj)
    //   $1 = (uintptr_t *) 0x7fa56d5fe360
    //   (gdb) p/x $mask = js::debug::GetMarkMask(obj, js::gc::GRAY)
    //   $2 = 0x200000000
    //   (gdb) watch *$word
    //   Hardware watchpoint 7: *$word
    //   (gdb) cond 7 *$word & $mask
    //   (gdb) cont
    //
    // Note that this is *not* a watchpoint on a single bit. It is a watchpoint on
    // the whole word, which will trigger whenever the word changes and the
    // selected bit is set after the change.
    //
    // So if the bit changing is the desired one, this is exactly what you want.
    // But if a different bit changes (either set or cleared), you may still stop
    // execution if the $mask bit happened to already be set. gdb does not expose
    // enough information to restrict the watchpoint to just a single bit.

Most of the time, you will want **js::gc::BLACK** (or you can just use 0) for the 2nd param to **js::debug::GetMarkMask**.
