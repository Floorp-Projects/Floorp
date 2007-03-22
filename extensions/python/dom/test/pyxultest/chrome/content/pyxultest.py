# The main script for the top-level pyxultest chrome.

import sys
import xpcom
from xpcom import components
import nsdom

# Utility functions
# Write something to the textbox - eg, error or trace messages.
def write( msg, *args):
    tb = document.getElementById("output_box")
    tb.value = tb.value + (msg % args) + "\n"

# An event listener class to test explicit hooking of events via objects.
# Note Python can now just call addEventListener a-la JS
class EventListener:
    _com_interfaces_ = components.interfaces.nsIDOMEventListener
    def __init__(self, handler, globs = None):
        try:
            self.co = handler.func_code
        except AttributeError:
            self.co = compile(handler, "inline script", "exec")
        self.globals = globs or globals()

    def handleEvent(self, event):
        exec self.co in self.globals

# A timer function
timer_id = None
timer_count = 0

def on_timer(max):
    global timer_count, timer_id
    assert timer_id is not None, "We must have a timer id!"
    change_image() # pick a random image
    timer_count += 1
    if timer_count >= max:
        write("Stopping the image timer (but clicking it will still change it)")
        window.clearTimeout(timer_id)
        time_id = None

# An event function to handle onload - but hooked up manually rather than via
# having the magic name 'onload'
def do_load():
    input = document.getElementById("output_box")
    # Clear the text in the XUL
    input.value = ""
    write("This is the Python on XUL demo using\nPython %s", sys.version)

    # hook up a click event using addEventListener
    button = document.getElementById("but_dialog")
    # Note the handler code is executed in the global scope of the targer, *not* the window.
    # Therefore objects in the global namespace must be prefixed with "window"
    button.addEventListener('click', 'write("hello from the click event for the dialog button")', False)

    # Test 'expandos' - set a custom attribute on a node, and check that
    # when a click event fires on it, the value is still there.
    button = document.getElementById("some-button")
    button.custom_value = "Python"
    # The event-handler.
    def check_expando(event):
        write("The custom value is %s", event.target.custom_value)
        if event.target.custom_value != "Python":
            write("but it is wrong!!!")

    button.addEventListener('click', check_expando, False)

    # And a 2 second 'interval' timer to change the image.
    global timer_id
    assert timer_id is None, "Already have a timer - event fired twice??"
    timer_id = window.setInterval(on_timer, 2000, 10)

# Add an event listener as a function
window.addEventListener('load', do_load, False)
# Add another one just to test passing a string instead of a function.
window.addEventListener('load', "dump('hello from string event handler')", False)
# And yet another with an explicit EventListener instance.
window.addEventListener('load', EventListener('dump("hello from an object event handler")'), False)

# Some other little functions called by the chrome
def on_but_dialog_click():
    write("Button clicked from %s", window.location.href)
    w = window.open("chrome://pyxultest/content/dialog.xul", "myDialog", "chrome")

def do_textbox_keypress(event):
    if event.keyCode==13:
        val = event.target.value
        if val:
            write('You wrote: %s', val)
        else:
            write("You wrote nothing!")
        event.target.value = ""

# An on-click handler for the image control
def change_image():
    image = document.getElementById("image-python");
    import random
    num = random.randrange(64) + 1
    url = "http://www.python.org/pics/PyBanner%03d.gif" % num
    image.src = url

def run_tests():
    # I wish I could reach into the window and get all tests!
    tests = """
                test_error_eventlistener_function
                test_error_eventlistener_function_noargs
                test_error_eventlistener_object
                test_error_eventlistener_string
                test_error_explicit
                test_error_explicit_no_cancel
                test_error_explicit_string
                test_interval_func
                test_timeout_func
                test_timeout_func_lateness
                test_timeout_string
                test_wrong_event_args
                """.split()
    keep_open = document.getElementById("keep_tests_open").getAttribute("checked")
    for test in tests:
        write("Running test %s", test)
        if keep_open:
            args = (test, "-k")
        else:
            args = (test,)
        window.openDialog("chrome://pyxultest/content/pytester.xul", "testDialog", "modal", *args)
    if keep_open:
        write("Ran all the tests - the windows told you if the tests worked")
    else:
        write("Ran all the tests - if no window stayed open, it worked!")
