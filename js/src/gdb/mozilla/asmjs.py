"""
In asm code, out-of-bounds heap accesses cause segfaults, which the engine
handles internally. Make GDB ignore them.
"""

import gdb

SIGSEGV = 11

# A sigaction buffer for each inferior process.
sigaction_buffers = {}


def on_stop(event):
    if isinstance(event, gdb.SignalEvent) and event.stop_signal == 'SIGSEGV':
        # Allocate memory for sigaction, once per js shell process.
        process = gdb.selected_inferior()
        buf = sigaction_buffers.get(process)
        if buf is None:
            buf = gdb.parse_and_eval("(struct sigaction *) malloc(sizeof(struct sigaction))")
            sigaction_buffers[process] = buf

        # See if WasmFaultHandler is installed as the SIGSEGV signal action.
        sigaction_fn = gdb.parse_and_eval('__sigaction')
        sigaction_fn(SIGSEGV, 0, buf)
        WasmFaultHandler = gdb.parse_and_eval("WasmFaultHandler")
        if buf['__sigaction_handler']['sa_handler'] == WasmFaultHandler:
            # Advise the user that magic is happening.
            print("js/src/gdb/mozilla/asmjs.py: Allowing WasmFaultHandler to run.")

            # If WasmFaultHandler doesn't handle this segfault, it will unhook
            # itself and re-raise.
            gdb.execute("continue")


def on_exited(event):
    if event.inferior in sigaction_buffers:
        del sigaction_buffers[event.inferior]


def install():
    gdb.events.stop.connect(on_stop)
    gdb.events.exited.connect(on_exited)
