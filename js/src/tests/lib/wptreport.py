# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integration between the jstests harness and `WptreportFormatter`.
#
# `WptreportFormatter` uses the data format specified in
# <https://firefox-source-docs.mozilla.org/mozbase/mozlog.html>.

from time import time

from wptrunner.formatters.wptreport import WptreportFormatter


class WptreportHandler(object):
    def __init__(self, out):
        """
        Initialize the WptreportHandler handler.

        :param str out: path to a file to write output to.
        """
        self.out = out
        self.formatter = WptreportFormatter()

    def suite_start(self):
        """
        Produce the "suite_start" message at the present time.
        """
        self.formatter.suite_start({
            "time": time(),
            "run_info": {},
        })

    def suite_end(self):
        """
        Produce the "suite_end" message at the present time and write the
        results to the file path given in the constructor.
        """
        result = self.formatter.suite_end({
            "time": time(),
        })
        with open(self.out, "w") as fp:
            fp.write(result)

    def test(self, result, duration):
        """
        Produce the "test_start", "test_status" and "test_end" messages, as
        appropriate.

        :param dict result: a dictionary with the test results. It should
                            include the following keys:
                            * "name": the ID of the test;
                            * "status": the actual status of the whole test;
                            * "expected": the expected status of the whole test;
                            * "subtests": a list of dicts with keys "test",
                              "subtest", "status" and "expected".
        :param float duration: the runtime of the test
        """
        testname = result["name"]

        end_time = time()
        start_time = end_time - duration

        self.formatter.test_start({
            "test": testname,
            "time": start_time,
        })

        for subtest in result["subtests"]:
            self.formatter.test_status(subtest)

        self.formatter.test_end({
            "test": testname,
            "time": end_time,
            "status": result["status"],
            "expected": result["expected"],
        })
