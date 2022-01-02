# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
In asm code, out-of-bounds heap accesses cause segfaults, which the engine
handles internally. Make GDB ignore them.
"""

import gdb

SIGSEGV = 11

# A sigaction buffer for each inferior process.
sigaction_buffers = {}


def on_stop(event):
    if isinstance(event, gdb.SignalEvent) and event.stop_signal == "SIGSEGV":
        # Allocate memory for sigaction, once per js shell process.
        process = gdb.selected_inferior()
        buf = sigaction_buffers.get(process)
        if buf is None:
            buf = gdb.parse_and_eval(
                "(struct sigaction *) malloc(sizeof(struct sigaction))"
            )
            sigaction_buffers[process] = buf

        # See if WasmFaultHandler is installed as the SIGSEGV signal action.
        sigaction_fn = gdb.parse_and_eval(
            "(void(*)(int,void*,void*))__sigaction"
        ).dereference()
        sigaction_fn(SIGSEGV, 0, buf)
        WasmTrapHandler = gdb.parse_and_eval("WasmTrapHandler")
        if buf["__sigaction_handler"]["sa_handler"] == WasmTrapHandler:
            # Advise the user that magic is happening.
            print("js/src/gdb/mozilla/asmjs.py: Allowing WasmTrapHandler to run.")

            # If WasmTrapHandler doesn't handle this segfault, it will unhook
            # itself and re-raise.
            gdb.execute("continue")


def on_exited(event):
    if event.inferior in sigaction_buffers:
        del sigaction_buffers[event.inferior]


def install():
    gdb.events.stop.connect(on_stop)
    gdb.events.exited.connect(on_exited)
