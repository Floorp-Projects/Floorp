# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A pywebsocket3 handler which runs arbitrary Python code and returns the
result.
This is used to test OS specific accessibility APIs which can't be tested in JS.
It is intended to be called from JS browser tests.
"""

import json
import os
import sys
import traceback

from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    def send(*args):
        """Send a response to the client as a JSON array."""
        msgutil.send_message(request, json.dumps(args))

    cleanNamespace = {}
    testDir = None
    if sys.platform == "win32":
        testDir = "windows"
    elif sys.platform == "linux":
        testDir = "atk"
    if testDir:
        sys.path.append(
            os.path.join(
                os.getcwd(), "browser", "accessible", "tests", "browser", testDir
            )
        )
        try:
            import a11y_setup

            cleanNamespace = a11y_setup.__dict__
            setupExc = None
        except Exception:
            setupExc = traceback.format_exc()
        sys.path.pop()

    def info(message):
        """Log an info message."""
        send("info", str(message))

    cleanNamespace["info"] = info
    namespace = cleanNamespace.copy()

    # Keep handling messages until the WebSocket is closed.
    while True:
        code = msgutil.receive_message(request)
        if not code:
            return
        if code == "__reset__":
            namespace = cleanNamespace.copy()
            continue

        if setupExc:
            # a11y_setup failed. Report an exception immediately.
            send("exception", setupExc)
            continue

        # Wrap the code in a function called run(). This allows the code to
        # return a result by simply using the return statement.
        if "\n" not in code and not code.lstrip().startswith("return "):
            # Single line without return. Assume this is an expression. We use
            # a lambda to return the result.
            code = f"run = lambda: {code}"
        else:
            lines = ["def run():"]
            # Indent each line inside the function.
            lines.extend(f"  {line}" for line in code.splitlines())
            code = "\n".join(lines)
        try:
            # Execute this Python code, which will define the run() function.
            exec(code, namespace)
            # Run the function we just defined.
            ret = namespace["run"]()
            send("return", ret)
        except Exception:
            send("exception", traceback.format_exc())
