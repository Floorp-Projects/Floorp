# Text progress bar library, like curl or scp.

import sys
from datetime import datetime

class NullProgressBar:
    def update(self, current, data): pass
    def poke(self): pass
    def finish(self, complete=True): pass
    def message(self, msg): sys.stdout.write(msg + '\n')

class ProgressBar:
    GREEN = '\x1b[32;1m'
    RED   = '\x1b[31;1m'
    BLUE  = '\x1b[34;1m'
    GRAY  = '\x1b[37;2m'

    RESET = '\x1b[0m'

    CLEAR_RIGHT = '\x1b[K'

    def __init__(self, limit, fmt):
        assert not self.conservative_isatty()
        assert limit < 9999

        self.prior = None
        self.counters_fmt = fmt # [{str:str}] Describtion of how to lay out each
                                #             field in the counters map.
        self.limit = limit # int: The value of 'current' equal to 100%.
        self.t0 = datetime.now() # datetime: The start time.

        # Compute the width of the counters and build the format string.
        self.counters_width = 1 # [
        for layout in self.counters_fmt:
            self.counters_width += 4 # Less than 9999 tests total.
            self.counters_width += 1 # | (or ']' for the last one)

        self.barlen = 64 - self.counters_width
        self.fmt = ('\r%-' + str(self.counters_width) + 's %3d%% %-' +
                    str(self.barlen) + 's| %6.1fs' + self.CLEAR_RIGHT)

    def update(self, current, data):
        # Record prior for poke.
        self.prior = (current, data)

        # Build counters string.
        counters = '['
        for layout in self.counters_fmt:
            counters += layout['color'] + ('%4d' % data[layout['value']]) + self.RESET
            counters += '|'
        counters = counters[:-1] + ']'

        # Build the bar.
        pct = int(100.0 * current / self.limit)
        barlen = int(1.0 * self.barlen * current / self.limit) - 1
        bar = '='*barlen + '>'

        # Update the bar.
        dt = datetime.now() - self.t0
        dt = dt.seconds + dt.microseconds * 1e-6
        sys.stdout.write(self.fmt % (counters, pct, bar, dt))

        # Force redisplay, since we didn't write a \n.
        sys.stdout.flush()

    def poke(self):
        if not self.prior:
            return
        self.update(*self.prior)

    def finish(self, complete=True):
        final_count = self.limit if complete else self.prior[0]
        self.update(final_count, self.prior[1])
        sys.stdout.write('\n')

    def message(self, msg):
        sys.stdout.write('\n')
        sys.stdout.write(msg)
        sys.stdout.write('\n')

    @staticmethod
    def conservative_isatty():
        """
        Prefer erring on the side of caution and not using terminal commands if
        the current output stream may be a file.  We explicitly check for the
        Android platform because terminal commands work poorly over ADB's
        redirection.
        """
        try:
            import android
        except ImportError:
            return False
        return sys.stdout.isatty()
