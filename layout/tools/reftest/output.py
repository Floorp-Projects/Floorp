# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import threading

from mozlog.formatters import TbplFormatter
from mozrunner.utils import get_stack_fixer_function


class ReftestFormatter(TbplFormatter):
    """
    Formatter designed to preserve the legacy "tbpl" format in reftest.

    This is needed for both the reftest-analyzer and mozharness log parsing.
    We can change this format when both reftest-analyzer and mozharness have
    been changed to read structured logs.
    """

    def __call__(self, data):
        if 'component' in data and data['component'] == 'mozleak':
            # Output from mozleak requires that no prefix be added
            # so that mozharness will pick up these failures.
            return "%s\n" % data['message']

        formatted = TbplFormatter.__call__(self, data)
        if data['action'] == 'process_output':
            return formatted
        return 'REFTEST %s' % formatted

    def log(self, data):
        prefix = "%s |" % data['level'].upper()
        return "%s %s\n" % (prefix, data['message'])

    def test_end(self, data):
        extra = data.get('extra', {})
        status = data['status']

        status_msg = "TEST-"
        if 'expected' in data:
            status_msg += "UNEXPECTED-%s" % status
        else:
            if status != "PASS":
                status_msg += "KNOWN-"
            status_msg += status
            if extra.get('status_msg') == 'Random':
                status_msg += "(EXPECTED RANDOM)"
        test = self.id_str(data['test'])
        if 'message' in data:
            status_details = data['message']
        elif isinstance(data['test'], tuple):
            status_details = 'image comparison ({})'.format(data['test'][1])
        else:
            status_details = '(LOAD ONLY)'

        output_text = "%s | %s | %s" % (status_msg, test, status_details)
        if 'differences' in extra:
            diff_msg = (", max difference: %(max_difference)s"
                        ", number of differing pixels: %(differences)s") % extra
            diagnostic_data = ("REFTEST   IMAGE 1 (TEST): %(image1)s\n"
                               "REFTEST   IMAGE 2 (REFERENCE): %(image2)s") % extra
            output_text += '%s\n%s' % (diff_msg, diagnostic_data)
        elif "image1" in extra:
            diagnostic_data = "REFTEST   IMAGE: %(image1)s" % extra
            output_text += '\n%s' % diagnostic_data

        output_text += "\nREFTEST TEST-END | %s" % test
        return "%s\n" % output_text

    def process_output(self, data):
        return "%s\n" % data["data"]

    def suite_end(self, data):
        lines = []
        summary = data['extra']['results']
        summary['success'] = summary['Pass'] + summary['LoadOnly']
        lines.append("Successful: %(success)s (%(Pass)s pass, %(LoadOnly)s load only)" %
                     summary)
        summary['unexpected'] = (summary['Exception'] + summary['FailedLoad'] +
                                 summary['UnexpectedFail'] + summary['UnexpectedPass'] +
                                 summary['AssertionUnexpected'] +
                                 summary['AssertionUnexpectedFixed'])
        lines.append(("Unexpected: %(unexpected)s (%(UnexpectedFail)s unexpected fail, "
                      "%(UnexpectedPass)s unexpected pass, "
                      "%(AssertionUnexpected)s unexpected asserts, "
                      "%(FailedLoad)s failed load, "
                      "%(Exception)s exception)") % summary)
        summary['known'] = (summary['KnownFail'] + summary['AssertionKnown'] +
                            summary['Random'] + summary['Skip'] + summary['Slow'])
        lines.append(("Known problems: %(known)s (" +
                      "%(KnownFail)s known fail, " +
                      "%(AssertionKnown)s known asserts, " +
                      "%(Random)s random, " +
                      "%(Skip)s skipped, " +
                      "%(Slow)s slow)") % summary)
        lines = ["REFTEST INFO | %s" % s for s in lines]
        lines.append("REFTEST SUITE-END | Shutdown")
        return "INFO | Result summary:\n{}\n".format('\n'.join(lines))

    def id_str(self, test_id):
        if isinstance(test_id, basestring):
            return test_id
        return test_id[0]


class OutputHandler(object):
    """Process the output of a process during a test run and translate
    raw data logged from reftest.js to an appropriate structured log action,
    where applicable.
    """

    def __init__(self, log, utilityPath, symbolsPath=None):
        self.stack_fixer_function = get_stack_fixer_function(utilityPath, symbolsPath)
        self.log = log
        # needed for b2gautomation.py
        self.suite_finished = False

    def __call__(self, line):
        # need to return processed messages to appease remoteautomation.py
        if not line.strip():
            return []

        try:
            data = json.loads(line)
        except ValueError:
            self.verbatim(line)
            return [line]

        if isinstance(data, dict) and 'action' in data:
            if data['action'] == 'suite_end':
                self.suite_finished = True

            self.log.log_raw(data)
        else:
            self.verbatim(json.dumps(data))

        return [data]

    def verbatim(self, line):
        if self.stack_fixer_function:
            line = self.stack_fixer_function(line)
        self.log.process_output(threading.current_thread().name, line)
