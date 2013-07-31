"""
The test performs the static code analysis check by JSHint.

Target js files:
- RILContentHelper.js
- RadioInterfaceLayer.js
- ril_worker.js
- ril_consts.js

If the js file contains the line of 'importScript()' (Ex: ril_worker.js), the
test will perform a special merge step before excuting JSHint.

Ex: Script A
--------------------------------
importScripts('Script B')
...
--------------------------------

We merge these two scripts into one by the following way.

--------------------------------
[Script B (ex: ril_consts.js)]
(function(){ [Script A (ex: ril_worker.js)]
})();
--------------------------------

Script A (ril_worker.js) runs global strict mode.
Script B (ril_consts.js) not.

The above merge way ensures the correct scope of 'strict mode.'
"""


from marionette_test import MarionetteTestCase
import bisect
import inspect
import os
import os.path
import re
import unicodedata


class StringUtility:

    """A collection of some string utilities."""

    @staticmethod
    def find_match_lines(lines, pattern):
        """Return a list of lines that contains given pattern."""
        return [line for line in lines if pattern in line]

    @staticmethod
    def remove_non_ascii(data):
        """Remove non ascii characters in data and return it as new string."""
        if type(data).__name__ == 'unicode':
            data = unicodedata.normalize(
                'NFKD', data).encode('ascii', 'ignore')
        return data

    @staticmethod
    def auto_close(lines):
        """Ensure every line ends with '\n'."""
        if lines and not lines[-1].endswith('\n'):
            lines[-1] += '\n'
        return lines

    @staticmethod
    def auto_wrap_strict_mode(lines):
        """Wrap by function scope if lines contain 'use strict'."""
        if StringUtility.find_match_lines(lines, 'use strict'):
            lines[0] = '(function(){' + lines[0]
            lines.append('})();\n')
        return lines

    @staticmethod
    def get_imported_list(lines):
        """Get a list of imported items."""
        return [item
                for line in StringUtility.find_match_lines(lines, 'importScripts')
                for item in StringUtility._get_imported_list_from_line(line)]

    @staticmethod
    def _get_imported_list_from_line(line):
        """Extract all items from 'importScripts(...)'.

        importScripts("ril_consts.js", "systemlibs.js")
        => ['ril_consts', 'systemlibs.js']

        """
        pattern = re.compile(r'\s*importScripts\((.*)\)')
        m = pattern.match(line)
        if not m:
            raise Exception('Parse importScripts error.')
        return [name.translate(None, '\' "') for name in m.group(1).split(',')]


class ResourceUriFileReader:

    """Handle the process of reading the source code from system."""

    URI_PREFIX = 'resource://gre/'
    URI_PATH = {
        'RILContentHelper.js':    'components/RILContentHelper.js',
        'RadioInterfaceLayer.js': 'components/RadioInterfaceLayer.js',
        'ril_worker.js':          'modules/ril_worker.js',
        'ril_consts.js':          'modules/ril_consts.js',
        'systemlibs.js':          'modules/systemlibs.js',
    }

    CODE_OPEN_CHANNEL_BY_URI = '''
    var Cc = SpecialPowers.Cc;
    var Ci = SpecialPowers.Ci;
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    global.uri = '%(uri)s';
    global.channel = ios.newChannel(global.uri, null, null);
    '''

    CODE_GET_SPEC = '''
    return global.channel.URI.spec;
    '''

    CODE_READ_CONTENT = '''
    var Cc = SpecialPowers.Cc;
    var Ci = SpecialPowers.Ci;

    var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
    var inputStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(Ci.nsIScriptableInputStream);

    var jaruri = global.channel.URI.QueryInterface(Ci.nsIJARURI);
    var file = jaruri.JARFile.QueryInterface(Ci.nsIFileURL).file;
    var entry = jaruri.JAREntry;
    zipReader.open(file);
    inputStream.init(zipReader.getInputStream(entry));
    var content = inputStream.read(inputStream.available());
    inputStream.close();
    zipReader.close();
    return content;
    '''

    @classmethod
    def get_uri(cls, filename):
        """Convert filename to URI in system."""
        return cls.URI_PREFIX + cls.URI_PATH[filename]

    def __init__(self, marionette):
        self.runjs = lambda x: marionette.execute_script(x, new_sandbox=False)

    def read_file(self, filename):
        """Read file and return the contents as string."""
        content = self._read_uri(self.get_uri(filename))
        content = content.replace('"use strict";', '')
        return StringUtility.remove_non_ascii(content)

    def _read_uri(self, uri):
        """Read URI in system and return the contents as string."""
        # Open the uri as a channel.
        self.runjs(self.CODE_OPEN_CHANNEL_BY_URI % {'uri': uri})

        # Make sure spec is a jar uri, and not recursive.
        #   Ex: 'jar:file:///system/b2g/omni.ja!/modules/ril_worker.js'
        #
        # For simplicity, we don't handle other special cases in this test.
        # If B2G build system changes in the future, such as put the jar in
        # another jar, the test case will fail.
        spec = self.runjs(self.CODE_GET_SPEC)
        if (not spec.startswith('jar:file://')) or (spec.count('jar:') != 1):
            raise Exception('URI resolve error')

        # Read the content from channel.
        content = self.runjs(self.CODE_READ_CONTENT)
        return content


class JSHintEngine:

    """Invoke jshint script on system."""

    CODE_INIT_JSHINT = '''
    %(script)s;
    global.JSHINT = JSHINT;
    global.options = JSON.parse(%(config_string)s);
    global.globals = global.options.globals;
    delete global.options.globals;
    '''

    CODE_RUN_JSHINT = '''
    global.script = %(code)s;
    return global.JSHINT(global.script, global.options, global.globals);
    '''

    CODE_GET_JSHINT_ERROR = '''
    return global.JSHINT.errors;
    '''

    def __init__(self, marionette, script, config):
        # Remove single line comment in config.
        config = '\n'.join([line.partition('//')[0]
                            for line in config.splitlines()])

        # Set global (JSHINT, options, global) in js environment.
        self.runjs = lambda x: marionette.execute_script(x, new_sandbox=False)
        self.runjs(self.CODE_INIT_JSHINT %
                   {'script': script, 'config_string': repr(config)})

    def run(self, code, filename=''):
        """Excute JShint check for the given code."""
        check_pass = self.runjs(self.CODE_RUN_JSHINT % {'code': repr(code)})
        errors = self.runjs(self.CODE_GET_JSHINT_ERROR)
        return check_pass, self._get_error_messages(errors, filename)

    def _get_error_messages(self, errors, filename=''):
        """
        Convert an error object to a list of readable string.

        [{"a": null, "c": null, "code": "W033", "d": null, "character": 6,
          "evidence": "var a", "raw": "Missing semicolon.",
          "reason": "Missing semicolon.", "b": null, "scope": "(main)", "line": 1,
          "id": "(error)"}]
        => line 1, col 6, Missing semicolon.

        """
        LINE, COL, REASON = u'line', u'character', u'reason'
        return ["%s: line %s, col %s, %s" %
                (filename, error[LINE], error[COL], error[REASON])
                for error in errors if error]


class Linter:

    """Handle the linting related process."""

    def __init__(self, code_reader, jshint, reporter=None):
        """Set the linter with code_reader, jshint engine, and reporter.

        Should have following functionality.
        - code_reader.read_file(filename)
        - jshint.run(code, filename)
        - reporter([...])

        """
        self.code_reader = code_reader
        self.jshint = jshint
        if reporter is None:
            self.reporter = lambda x: '\n'.join(x)
        else:
            self.reporter = reporter

    def lint_file(self, filename):
        """Lint the file and return (pass, error_message)."""
        # Get code contents.
        code = self.code_reader.read_file(filename)
        lines = code.splitlines()
        import_list = StringUtility.get_imported_list(lines)
        if not import_list:
            check_pass, error_message = self.jshint.run(code, filename)
        else:
            newlines, info = self._merge_multiple_codes(filename, import_list)
            # Each line of |newlines| contains '\n'.
            check_pass, error_message = self.jshint.run(''.join(newlines))
            error_message = self._convert_merged_result(error_message, info)
            # Only keep errors for this file.
            error_message = [line for line in error_message
                             if line.startswith(filename)]
            check_pass = (len(error_message) == 0)
        return check_pass, self.reporter(error_message)

    def _merge_multiple_codes(self, filename, import_list):
        """Merge multiple codes from filename and import_list."""
        dirname, filename = os.path.split(filename)
        dst_line = 1
        dst_results = []
        info = []

        # Put the imported script first, and then the original script.
        for f in import_list + [filename]:
            filepath = os.path.join(dirname, f)

            # Maintain a mapping table.
            # New line number after merge => original file and line number.
            info.append((dst_line, filepath, 1))

            code = self.code_reader.read_file(filepath)
            lines = code.splitlines(True)  # Keep '\n'.
            src_results = StringUtility.auto_wrap_strict_mode(
                StringUtility.auto_close(lines))
            dst_results.extend(src_results)
            dst_line += len(src_results)
        return dst_results, info

    def _convert_merged_result(self, error_lines, line_info):
        pattern = re.compile(r'(.*): line (\d+),(.*)')
        start_line = [info[0] for info in line_info]
        new_result_lines = []
        for line in error_lines:
            m = pattern.match(line)
            if not m:
                continue

            line_number, remain = int(m.group(2)), m.group(3)

            #  [1, 2, 7, 8]
            #            ^ for 7, pos = 3
            #         ^    for 6, pos = 2
            pos = bisect.bisect_right(start_line, line_number)
            dst_line, name, src_line = line_info[pos - 1]
            real_line_number = line_number - dst_line + src_line
            new_result_lines.append(
                "%s: line %s,%s" % (name, real_line_number, remain))
        return new_result_lines


class TestRILCodeQuality(MarionetteTestCase):

    JSHINT_PATH = 'ril_jshint/jshint.js'
    JSHINTRC_PATH = 'ril_jshint/jshintrc'

    def _read_local_file(self, filepath):
        """Read file content from local (folder of this test case)."""
        test_dir = os.path.dirname(inspect.getfile(TestRILCodeQuality))
        return open(os.path.join(test_dir, filepath)).read()

    def _get_extended_error_message(self, error_message):
        return '\n'.join(['See errors below and more information in Bug 880643',
                          '\n'.join(error_message),
                          'See errors above and more information in Bug 880643'])

    def _check(self, filename):
        check_pass, error_message = self.linter.lint_file(filename)
        self.assertTrue(check_pass, error_message)

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.linter = Linter(
            ResourceUriFileReader(self.marionette),
            JSHintEngine(self.marionette,
                         self._read_local_file(self.JSHINT_PATH),
                         self._read_local_file(self.JSHINTRC_PATH)),
            self._get_extended_error_message)

    def tearDown(self):
        MarionetteTestCase.tearDown(self)

    def test_RILContentHelper(self):
        self._check('RILContentHelper.js')

    def test_RadioInterfaceLayer(self):
        self._check('RadioInterfaceLayer.js')

    def test_ril_worker(self):
        self._check('ril_worker.js')

    def test_ril_consts(self):
        self._check('ril_consts.js')
