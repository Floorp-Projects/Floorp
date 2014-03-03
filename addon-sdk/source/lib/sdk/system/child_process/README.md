# subprocess


Created by [Jan Gerber](j@mailb.org), with contributions from [Patrick Brunschwig](patrick@enigmail.net), subprocess.jsm is used as the underlying library, supporting the Addon-SDK's implementation of Node's `child-process` API.

`subprocess.jsm` is originally from [http://hg.mozilla.org/ipccode/](http://hg.mozilla.org/ipccode/).

How to Use subprocess.jsm in your Add-on
----------------------------------------

1. copy subprocess.jsm and subprocess_worker_*.js into the modules/ directory
   of your add-on.

2. add this line to chrome.manifest:
 resource EXTENSION modules/

3. import it where needed:
 Components.utils.import("resource://EXTENSION/subprocess.jsm");

This object allows to start a process, and read/write data to/from it
using stdin/stdout/stderr streams.

Usage example:

  var p = subprocess.call({
    command: '/bin/foo',
    arguments: ['-v', 'foo'],
    environment: [ "XYZ=abc", "MYVAR=def" ],
    charset: 'UTF-8',
    workdir: '/home/foo',
    //stdin: "some value to write to stdin\nfoobar",
    stdin: function(stdin) {
      stdin.write("some value to write to stdin\nfoobar");
      stdin.close();
    },
    stdout: function(data) {
      dump("got data on stdout:" + data + "\n");
    },
    stderr: function(data) {
      dump("got data on stderr:" + data + "\n");
    },
    done: function(result) {
      dump("process terminated with " + result.exitCode + "\n");
    },
    mergeStderr: false
  });

  p.wait(); // wait for the subprocess to terminate,
            // this will block the main thread,
            // only do if you can wait that long


Description of subprocess.call(...) Parameters
----------------------------------------------
Apart from <command>, all arguments are optional.

 command: either a |nsIFile| object pointing to an executable file or a
              String containing the platform-dependent path to an executable
              file.

 arguments: optional string array containing the arguments to the command.

 environment: optional string array containing environment variables to pass
              to the command. The array elements must have the form
              "VAR=data". Please note that if environment is defined, it
              replaces any existing environment variables for the subprocess.

 charset: Output is decoded with given charset and a string is returned.
              If charset is undefined, "UTF-8" is used as default.
              To get binary data, set this to null and the returned string
              is not decoded in any way.

 workdir: Optional; either a |nsIFile| object or string containing the
              platform-dependent path to a directory to become the current
              working directory of the subprocess.

 stdin: Optional input data for the process to be passed on standard
              input. stdin can either be a string or a function.
              A |string| gets written to stdin and stdin gets closed;
              A |function| gets passed an object with write and close function.
              Please note that the write() function will return almost immediately;
              data is always written asynchronously on a separate thread.

 stdout: An optional function that can receive output data from the
              process. The stdout-function is called asynchronously; it can be
              called mutliple times during the execution of a process.
              At a minimum at each occurance of \n or \r.
              Please note that null-characters might need to be escaped
              with something like 'data.replace(/\0/g, "\\0");'.

 stderr: An optional function that can receive stderr data from the
              process. The stderr-function is called asynchronously; it can be
              called mutliple times during the execution of a process. Please
              note that null-characters might need to be escaped with
              something like 'data.replace(/\0/g, "\\0");'.
              (on windows it only gets called once right now)

 done: Optional function that is called when the process has terminated.
              The exit code from the process available via result.exitCode. If
              stdout is not defined, then the output from stdout is available
              via result.stdout. stderr data is in result.stderr

 mergeStderr: Optional boolean value. If true, stderr is merged with stdout;
              no data will be provided to stderr.


Description of object returned by subprocess.call(...)
------------------------------------------------------
The object returned by subprocess.call offers a few methods that can be
executed:

 wait(): waits for the subprocess to terminate. It is not required to use
              wait; done will be called in any case when the subprocess terminated.

 kill(): kill the subprocess. Any open pipes will be closed and
              done will be called.


Other methods exported by subprocess
------------------------------------
The following functions help debugging and provide logging facilities.

 registerDebugHandler(functionRef): register a handler that is called to get
                                      debugging information
 registerLogHandler(functionRef): register a handler that is called to get error
                                      messages

 example:
    subprocess.registerLogHandler( function(s) { dump(s); } );
