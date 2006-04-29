"""
Runs 'automated' tests - generally anything which can be tested without
visual confirmation of the results.

This is generally executed by the main pyxultest chrome, passing the name
of a test to run.  If the test succeeds, the window closes before it is
shown - hence the requirement that only things that don't need visual
confirmation.

For testing purposes, you can specify:
   -chrome chrome://pyxultest/content/pytester.xul -test test_name
And have a specific test executed from the command-line.

Executing without a -test option will print the name of all tests, then
execute one at random.
"""

import sys
from xpcom import components
import xpcom

close_on_success = True

# Utility functions
# Write something to the textbox - eg, error or trace messages.
def write( msg, *args):
    tb = document.getElementById("output-box")
    tb.value = tb.value + (msg % args) + "\n"

def success():
    # This gets called if the test works.  If it is not called, it must not
    # have worked :)
    write("The test worked!")
    if close_on_success:
        dump("Closing the window - nothing interesting to see here!")
        window.close()

def cause_error(expect_cancel = True):
    write("Raising an error")
    if expect_cancel:
        raise RuntimeError, \
              "This error was cancelled - you should not have seen it!"
    raise RuntimeError, \
          "Testing an unhandled exception - you should see this!"

def handle_error_return_cancel(errMsg, source, lineno):
    success()
    return True # cancel the error

# This is set as an event handler, but it has the wrong number of args.  It
# should still work, a-la js.
def handle_error_wrong_args(event):
    success()
    return True # cancel this error

def handle_error_no_args():
    success()

def test_error_explicit():
    "Test an explicit assignment of a function object to onerror"
    # Assign a function to an onerror attribute
    write("Assigning a function to window.onerror")
    window.onerror = handle_error_return_cancel
    cause_error()

def test_error_explicit_string():
    "Test an explicit assignment of a string to onerror"
    # Assign a string to an onerror attribute
    write("Assigning a string to window.onerror")
    window.onerror = "return handle_error_return_cancel(event, source, lineno)"
    cause_error()

def test_error_explicit_no_cancel():
    """Test an error we don't handle"""
    # NOTE: No 'return' - so we return None - ie, not cancelled.
    window.onerror = "handle_error_wrong_args(event)"
    cause_error(False)

def test_wrong_event_args():
    "Test an error handler with too few args"
    # Assign a string to an onerror attribute
    write("Assigning a function taking only 1 arg to window.onerror")
    window.onerror = handle_error_wrong_args
    cause_error()

# A test class for use with addEventListener.
class EventListener:
    _com_interfaces_ = components.interfaces.nsIDOMEventListener
    def handleEvent(self, event):
        write("nsIDOMEventListener caught the event")
        success()
        # return value ignored for handleEvent - must call preventDefault.
        event.preventDefault()

def test_error_eventlistener_object():
    """Test we can hook the error with our own nsIDOMEventListener"""
    write("Calling addEventListener with an nsIDOMEventListener instance")
    window.addEventListener("error", EventListener(), False)
    cause_error()

def handle_error_explicit_cancel(event):
    success()
    event.preventDefault()

# addEventListener for a string and function - these use the EventListener
# impl in nsdom.context
def test_error_eventlistener_function():
    """Test we can hook the error by passing addEventListener a function"""
    write("Calling addEventListener with a function object")
    window.addEventListener("error", handle_error_explicit_cancel, False)
    cause_error()

def test_error_eventlistener_function_noargs():
    """Test we can addEventListener a function taking no args"""
    write("Calling addEventListener with a function object")
    window.addEventListener("error", handle_error_no_args, False)
    # handle_error_no_args can't cancel the event as it is not passed - the
    # return value is ignored for addEventListener functions.
    cause_error(False)

def test_error_eventlistener_string():
    """Test we can hook the error by passing addEventListener a string"""
    write("Calling addEventListener with a string")
    window.addEventListener("error", "event.preventDefault();success()", False)
    cause_error()

def find_tests():
    ret = {}
    for name, val in globals().items():
        if callable(val) and name.startswith("test") and val.__doc__:
            ret[name] = val
    return ret

#
# Timeout and Interval tests.
#
timer_id = None
_interval_counter = 0
def interval_func():
    global _interval_counter
    _interval_counter += 1
    if _interval_counter > 1:
        window.clearInterval(timer_id)
        write("Interval timer fired twice - looks good!")
        success()
    
def test_interval_func():
    "Test setInterval function"
    global timer_id
    write("Assigning an interval using a function")
    timer_id = window.setInterval(interval_func, 100)

def timer_func(arg1, arg2):
    # Also checks we can pass arbitrary Python objects.
    if arg1 == "hello" and arg2 is test_timeout_func:
        success()
    else:
        write("Args seemed wrong - test failed!")

def timer_func_lateness(lateness):
    # Also checks we can pass arbitrary Python objects.
    write("lateness is %r", lateness)
    # Check value reasonbleness - we should always be an int and never more
    # than a second late.
    if type(lateness) == type(0) and lateness < 1000:
        success()
    else:
        write("Eeek - lateness is %r", lateness)

def test_timeout_func():
    "Test setTimeout function and general arg handling"
    write("Assigning an interval using a function")
    timer_id = window.setTimeout(100, timer_func, "hello", test_timeout_func)

def test_timeout_string():
    "Test setTimeout function with a string"
    write("Assigning an interval using a function")
    timer_id = window.setTimeout(100, "write('The timeout string fired'); success()")

def test_timeout_func_lateness():
    "Test setTimeout function handling of 'lateness' arg"
    write("Assigning an interval using a function")
    timer_id = window.setTimeout(100, timer_func_lateness)


# The general event handlers and test runner code.
def do_onload(event):
    global close_on_success
    klasses = []

    # window.arguments may specify the name of a test to run - this is the
    # case when opened from our main XUL demo form.

    # When this form is executed directly via '--chrome ...', for some reason
    # the nsAppRunner always passes an empty string as an arg.
    # So we detect this, and try to find a '-test' option specifying
    # the test to run.
    tests = find_tests()
    func_name = None
    if len(window.arguments)==1 and not window.arguments[0]:
        try:
            # This is an embedding component - it may not always be there.
            contract_id = "@mozilla.org/app-startup/commandLineService;1"
            cmdline_svc = components.classes[contract_id] \
                           .getService(components.interfaces.nsICmdLineService)
            func_name = cmdline_svc.getCmdLineValue("-test")
            if func_name is None:
                write("You can specify '-test test_name' on the command-line" \
                      " to specify a test to run")
                write("The following tests can be specified:")
                names = tests.keys()
                names.sort()
                for name in names:
                    write("  " + name)
            close_on_success = False
        except xpcom.COMException:
            write("No command-line service - can't check for cmdline test")
    elif len(window.arguments) and window.arguments[0]:
        func_name = window.arguments[0]

    func = None
    if func_name:
        try:
            func = tests[func_name]
        except KeyError:
            write("*** No such test '%s' - running default test" % func_name)
            write("The following tests can be specified:")
            for name in tests:
                write("  " + name)
            func = None
    if func is None:
        close_on_success = False
        import random
        func = random.choice(tests.values())
        write("Running random test '%s'" % (func.func_name,))

    if len(window.arguments)>1 and window.arguments[1]=="-k":
        close_on_success = False
    if func:
        write("%s: %s" % (func.__name__, func.__doc__ or func.__name__))
        func()
