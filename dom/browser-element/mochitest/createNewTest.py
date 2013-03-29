"""A script to generate the browser-element test boilerplate.

This script requires Python 2.7."""

from __future__ import print_function

import sys
import os
import stat
import argparse
import textwrap
import subprocess

html_template = textwrap.dedent("""\
    <!DOCTYPE HTML>
    <html>
    <head>
      <title>Test for Bug {bug}</title>
      <script type="application/javascript" src="/tests/SimpleTest/SimpleTest.js"></script>
      <script type="application/javascript" src="browserElementTestHelpers.js"></script>
      <link rel="stylesheet" type="text/css" href="/tests/SimpleTest/test.css"/>
    </head>
    <body>
    <script type="application/javascript;version=1.7" src="browserElement_{test}.js">
    </script>
    </body>
    </html>""")

# Note: Curly braces are escaped as "{{".
js_template = textwrap.dedent("""\
    /* Any copyright is dedicated to the public domain.
       http://creativecommons.org/publicdomain/zero/1.0/ */

    // Bug {bug} - FILL IN TEST DESCRIPTION
    "use strict";

    SimpleTest.waitForExplicitFinish();
    browserElementTestHelpers.setEnabledPref(true);
    browserElementTestHelpers.addPermission();

    function runTest() {{
      var iframe = document.createElement('iframe');
      SpecialPowers.wrap(iframe).mozbrowser = true;

      // FILL IN TEST

      document.body.appendChild(iframe);
    }}

    addEventListener('testready', runTest);
    """)

def print_fill(s):
    print(textwrap.fill(textwrap.dedent(s)))

def add_to_makefile(filenames):
    """Add a list of filenames to this directory's Makefile.in, then open
    $EDITOR and let the user move the filenames to their appropriate places in
    the file.

    """
    lines_to_write = [''] + ['\t\t%s \\' % n for n in filenames]
    with open('Makefile.in', 'a') as f:
        f.write('\n'.join(lines_to_write))

    if 'EDITOR' not in os.environ or not os.environ['EDITOR']:
        print_fill("""\
            Now open Makefile.in and move the filenames to their correct places.")
            (Define $EDITOR and I'll open your editor for you next time.)""")
        return

    # Count the number of lines in Makefile.in.
    with open('Makefile.in', 'r') as f:
        num_lines = len(f.readlines())

    try:
        subprocess.call([os.environ['EDITOR'],
                         '+%d' % (num_lines - len(lines_to_write) + 2),
                         'Makefile.in'])
    except Exception as e:
        print_fill("Error opening $EDITOR: %s." % str(e))
        print()
        print_fill("""\
            Please open Makefile.in and move the filenames at the bottom of the
            file to their correct places.""")

def main(test_name, bug_number):
    global html_template, js_template

    def format(str):
        return str.format(bug=bug_number, test=test_name)

    def create_file(filename, template):
        path = os.path.join(os.path.dirname(sys.argv[0]), format(filename))
        # Create a new file, bailing with an error if the file exists.
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL)

        try:
            # This file has 777 permission when created, for whatever reason.  Make it rw-rw-r---.
            os.fchmod(fd, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IWGRP | stat.S_IROTH)
        except:
            # fchmod doesn't work on Windows.
            pass

        with os.fdopen(fd, 'w') as file:
            file.write(format(template))

    create_file('browserElement_{test}.js', js_template)
    create_file('test_browserElement_inproc_{test}.html', html_template)
    create_file('test_browserElement_oop_{test}.html', html_template)

    add_to_makefile([format(x) for x in ['browserElement_{test}.js',
                                         'test_browserElement_inproc_{test}.html',
                                         'test_browserElement_oop_{test}.html']])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Create a new browser-element testcase.")
    parser.add_argument('test_name')
    parser.add_argument('bug_number', type=int)
    args = parser.parse_args()
    main(args.test_name, args.bug_number)
