import sys
import threading
import time

# XXX We want to convince ourselves that the race condition this file works
# around, tracked by bug 1046873, is gone, and get rid of this file entirely
# before we hook this up to Tbpl, so that we don't introduce an intermittent
# failure.
#
# reduced down from <https://gist.github.com/niccokunzmann/6038331>; importing
# this class just so happens to allow mach to exit rather than hanging after
# our tests are run.


def monitor():
    while 1:
        time.sleep(1.)
        sys._current_frames()


def start_monitoring():
    thread = threading.Thread(target=monitor)
    thread.daemon = True
    thread.start()
    return thread
