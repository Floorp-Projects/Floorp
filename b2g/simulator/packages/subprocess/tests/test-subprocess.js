/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { Cc, Ci } = require("chrome");
const subprocess = require("subprocess");
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

// For now, only test on windows
if (env.get('OS') && env.get('OS').match(/Windows/)) {

exports.testWindows = function (test) {
  test.waitUntilDone();
  let envTestValue = "OK";
  let gotStdout = false;

  var p = subprocess.call({
    // Retrieve windows cmd.exe path from env
    command:     env.get('ComSpec'),
    // In order to execute a simple "echo" function
    arguments:   ['/C', 'echo %ENV_TEST%'], // ' & type CON' should display stdin, but doesn't work
    // Printing an environnement variable set here by the parent process
    environment: ['ENV_TEST='+envTestValue],

    stdin: function(stdin) {
      // Win32 command line is not really made for stdin
      // So it doesn't seems to work as it's hard to retrieve stdin
      stdin.write("stdin");
      stdin.close();
    },
    stdout: function(data) {
      test.assert(!gotStdout,"don't get stdout twice");
      test.assertEqual(data,envTestValue+"\r\n","stdout contains the environment variable");
      gotStdout = true;
    },
    stderr: function(data) {
      test.fail("shouldn't get stderr");
    },
    done: function() {
      test.assert(gotStdout, "got stdout before finished");
      test.done();
    },
    mergeStderr: false
  });

}

exports.testWindowsStderr = function (test) {
  test.waitUntilDone();
  let gotStderr = false;

  var p = subprocess.call({
    command:     env.get('ComSpec'),
    arguments:   ['/C', 'nonexistent'],

    stdout: function(data) {
      test.fail("shouldn't get stdout");
    },
    stderr: function(data) {
      test.assert(!gotStderr,"don't get stderr twice");
      test.assertEqual(
        data,
        "'nonexistent' is not recognized as an internal or external command,\r\n" +
        "operable program or batch file.\r\n",
        "stderr contains the error message"
      );
      gotStderr = true;
    },
    done: function() {
      test.assert(gotStderr, "got stderr before finished");
      test.done();
    },
    mergeStderr: false
  });

}

}

if (env.get('USER') && env.get('SHELL')) {

exports.testUnix = function (test) {
  test.waitUntilDone();
  let envTestValue = "OK";
  let gotStdout = false;

  var p = subprocess.call({
    command:     '/bin/sh',
    // Print stdin and our env variable
    //arguments:   ['-c', 'echo $@ $ENV_TEST'],
    environment: ['ENV_TEST='+envTestValue],

    stdin: function(stdin) {
      stdin.write("echo $ENV_TEST");
      stdin.close();
    },
    stdout: function(data) {
      test.assert(!gotStdout,"don't get stdout twice");
      test.assertEqual(data,envTestValue+"\n","stdout contains the environment variable");
      gotStdout = true;
    },
    stderr: function(data) {
      test.fail("shouldn't get stderr");
    },
    done: function() {
      test.assert(gotStdout, "got stdout before finished");
      test.done();
    },
    mergeStderr: false
  });
}

exports.testUnixStderr = function (test) {
  test.waitUntilDone();
  let gotStderr = false;

  var p = subprocess.call({
    // Hope that we don't have to give absolute path on linux ...
    command:     '/bin/sh',
    arguments:   ['nonexistent'],

    stdout: function(data) {
      test.fail("shouldn't get stdout");
    },
    stderr: function(data) {
      test.assert(!gotStderr,"don't get stderr twice");
      // There is two variant of error message
      if (data == "/bin/sh: 0: Can't open nonexistent\n")
        test.pass("stderr containes the expected error message");
      else
        test.assertEqual(data, "/bin/sh: nonexistent: No such file or directory\n",
                         "stderr contains the error message");
      gotStderr = true;
    },
    done: function() {
      test.assert(gotStderr, "got stderr before finished");
      test.done();
    },
    mergeStderr: false
  });
}

}
