# Text progress bar library, like curl or scp.

import sys
from datetime import datetime

if sys.platform.startswith('win'):
    from lib.terminal_win import Terminal
else:
    from lib.terminal_unix import Terminal

class NullProgressBar(object):
    def update(self, current, data): pass
    def poke(self): pass
    def finish(self, complete=True): pass
    def message(self, msg): sys.stdout.write(msg + '\n')

class ProgressBar(object):
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

    def update(self, current, data):
        # Record prior for poke.
        self.prior = (current, data)

        # Build counters string.
        sys.stdout.write('\r[')
        for layout in self.counters_fmt:
            Terminal.set_color(layout['color'])
            sys.stdout.write('%4d' % data[layout['value']])
            Terminal.reset_color()
            if layout != self.counters_fmt[-1]:
                sys.stdout.write('|')
            else:
                sys.stdout.write('] ')

        # Build the bar.
        pct = int(100.0 * current / self.limit)
        sys.stdout.write('%3d%% ' % pct)

        barlen = int(1.0 * self.barlen * current / self.limit) - 1
        bar = '=' * barlen + '>' + ' ' * (self.barlen - barlen - 1)
        sys.stdout.write(bar + '|')

        # Update the bar.
        dt = datetime.now() - self.t0
        dt = dt.seconds + dt.microseconds * 1e-6
        sys.stdout.write('%6.1fs' % dt)
        Terminal.clear_right()

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
