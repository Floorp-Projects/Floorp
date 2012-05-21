# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Text progress bar library, like curl or scp.

import sys, datetime

class ProgressBar:
    def __init__(self, label, limit, label_width=12):
        self.label = label
        self.limit = limit
        self.label_width = label_width
        self.cur = 0
        self.t0 = datetime.datetime.now()

        self.barlen = 64 - self.label_width
        self.fmt = '\r%-' + str(label_width) + 's %3d%% %-' + str(self.barlen) + 's| %6.1fs'

    def update(self, value):
        self.cur = value
        pct = int(100.0 * self.cur / self.limit)
        barlen = int(1.0 * self.barlen * self.cur / self.limit) - 1
        bar = '='*barlen + '>'
        dt = datetime.datetime.now() - self.t0
        dt = dt.seconds + dt.microseconds * 1e-6
        sys.stdout.write(self.fmt%(self.label[:self.label_width], pct, bar, dt))
        sys.stdout.flush()

    def finish(self):
        self.update(self.limit)
        sys.stdout.write('\n')

if __name__ == '__main__':
    pb = ProgressBar('test', 12)
    for i in range(12):
        pb.update(i)
        time.sleep(0.5)
    pb.finish()
