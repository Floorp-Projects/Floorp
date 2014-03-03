/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental'
};

let { Ci } = require('chrome');
let subprocess = require('./child_process/subprocess');
let { EventTarget } = require('../event/target');
let { Stream } = require('../io/stream');
let { on, emit, off } = require('../event/core');
let { Class } = require('../core/heritage');
let { platform } = require('../system');
let { isFunction, isArray } = require('../lang/type');
let { delay } = require('../lang/functional');
let { merge } = require('../util/object');
let { setTimeout, clearTimeout } = require('../timers');
let isWindows = platform.indexOf('win') === 0;

let processes = WeakMap();


/**
 * The `Child` class wraps a subprocess command, exposes
 * the stdio streams, and methods to manipulate the subprocess
 */
let Child = Class({
  implements: [EventTarget],
  initialize: function initialize (options) {
    let child = this;
    let proc;

    this.killed = false;
    this.exitCode = undefined;
    this.signalCode = undefined;

    this.stdin = Stream();
    this.stdout = Stream();
    this.stderr = Stream();

    try {
      proc = subprocess.call({
        command: options.file,
        arguments: options.cmdArgs,
        environment: serializeEnv(options.env),
        workdir: options.cwd,
        charset: options.encoding,
        stdout: data => emit(child.stdout, 'data', data),
        stderr: data => emit(child.stderr, 'data', data),
        stdin: stream => {
          child.stdin.on('data', pumpStdin);
          child.stdin.on('end', function closeStdin () {
            child.stdin.off('data', pumpStdin);
            child.stdin.off('end', closeStdin);
            stream.close();
          });
          function pumpStdin (data) {
            stream.write(data);
          }
        },
        done: function (result) {
          // Only emit if child is not killed; otherwise,
          // the `kill` method will handle this
          if (!child.killed) {
            child.exitCode = result.exitCode;
            child.signalCode = null;

            // If process exits with < 0, there was an error
            if (child.exitCode < 0) {
              handleError(new Error('Process exited with exit code ' + child.exitCode));
            }
            else {
              // Also do 'exit' event as there's not much of
              // a difference in our implementation as we're not using
              // node streams
              emit(child, 'exit', child.exitCode, child.signalCode);
            }

            // Emit 'close' event with exit code and signal,
            // which is `null`, as it was not a killed process
            emit(child, 'close', child.exitCode, child.signalCode);
          }
        }
      });
      processes.set(child, proc);
    } catch (e) {
      // Delay the error handling so an error handler can be set
      // during the same tick that the Child was created
      delay(() => handleError(e));
    }

    // `handleError` is called when process could not even
    // be spawned
    function handleError (e) {
      // If error is an nsIObject, make a fresh error object
      // so we're not exposing nsIObjects, and we can modify it
      // with additional process information, like node
      let error = e;
      if (e instanceof Ci.nsISupports) {
        error = new Error(e.message, e.filename, e.lineNumber);
      }
      emit(child, 'error', error);
      child.exitCode = -1;
      child.signalCode = null;
      emit(child, 'close', child.exitCode, child.signalCode);
    }
  },
  kill: function kill (signal) {
    let proc = processes.get(this);
    proc.kill(signal);
    this.killed = true;
    this.exitCode = null;
    this.signalCode = signal;
    emit(this, 'exit', this.exitCode, this.signalCode);
    emit(this, 'close', this.exitCode, this.signalCode);
  },
  get pid() { return processes.get(this, {}).pid || -1; }
});

function spawn (file, ...args) {
  let cmdArgs = [];
  // Default options
  let options = {
    cwd: null,
    env: null,
    encoding: 'UTF-8'
  };

  if (args[1]) {
    merge(options, args[1]);
    cmdArgs = args[0];
  }
  else {
    if (isArray(args[0]))
      cmdArgs = args[0];
    else
      merge(options, args[0]);
  }

  if ('gid' in options)
    console.warn('`gid` option is not yet supported for `child_process`');
  if ('uid' in options)
    console.warn('`uid` option is not yet supported for `child_process`');
  if ('detached' in options)
    console.warn('`detached` option is not yet supported for `child_process`');

  options.file = file;
  options.cmdArgs = cmdArgs;

  return Child(options);
}

exports.spawn = spawn;

/**
 * exec(command, options, callback)
 */
function exec (cmd, ...args) {
  let file, cmdArgs, callback, options = {};

  if (isFunction(args[0]))
    callback = args[0];
  else {
    merge(options, args[0]);
    callback = args[1];
  }

  if (isWindows) {
    file = 'C:\\Windows\\System32\\cmd.exe';
    cmdArgs = ['/s', '/c', (cmd || '').split(' ')];
  }
  else {
    file = '/bin/sh';
    cmdArgs = ['-c', cmd];
  }

  // Undocumented option from node being able to specify shell
  if (options && options.shell)
    file = options.shell;

  return execFile(file, cmdArgs, options, callback);
}
exports.exec = exec;
/**
 * execFile (file, args, options, callback)
 */
function execFile (file, ...args) {
  let cmdArgs = [], callback;
  // Default options
  let options = {
    cwd: null,
    env: null,
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: 200 * 1024,
    killSignal: 'SIGTERM'
  };

  if (isFunction(args[args.length - 1]))
    callback = args[args.length - 1];

  if (isArray(args[0])) {
    cmdArgs = args[0];
    merge(options, args[1]);
  } else if (!isFunction(args[0]))
    merge(options, args[0]);

  let child = spawn(file, cmdArgs, options);
  let exited = false;
  let stdout = '';
  let stderr = '';
  let error = null;
  let timeoutId = null;

  child.stdout.setEncoding(options.encoding);
  child.stderr.setEncoding(options.encoding);

  on(child.stdout, 'data', pumpStdout);
  on(child.stderr, 'data', pumpStderr);
  on(child, 'close', exitHandler);
  on(child, 'error', errorHandler);

  if (options.timeout > 0) {
    setTimeout(() => {
      kill();
      timeoutId = null;
    }, options.timeout);
  }

  function exitHandler (code, signal) {

    // Return if exitHandler called previously, occurs
    // when multiple maxBuffer errors thrown and attempt to kill multiple
    // times
    if (exited) return;
    exited = true;

    if (!isFunction(callback)) return;

    if (timeoutId) {
      clearTimeout(timeoutId);
      timeoutId = null;
    }

    if (!error && (code !== 0 || signal !== null))
      error = createProcessError(new Error('Command failed: ' + stderr), {
        code: code,
        signal: signal,
        killed: !!child.killed
      });

    callback(error, stdout, stderr);

    off(child.stdout, 'data', pumpStdout);
    off(child.stderr, 'data', pumpStderr);
    off(child, 'close', exitHandler);
    off(child, 'error', errorHandler);
  }

  function errorHandler (e) {
    error = e;
    exitHandler();
  }

  function kill () {
    try {
      child.kill(options.killSignal);
    } catch (e) {
      // In the scenario where the kill signal happens when
      // the process is already closing, just abort the kill fail
      if (/library is not open/.test(e))
        return;
      error = e;
      exitHandler(-1, options.killSignal);
    }
  }

  function pumpStdout (data) {
    stdout += data;
    if (stdout.length > options.maxBuffer) {
      error = new Error('stdout maxBuffer exceeded');
      kill();
    }
  }

  function pumpStderr (data) {
    stderr += data;
    if (stderr.length > options.maxBuffer) {
      error = new Error('stderr maxBuffer exceeded');
      kill();
    }
  }

  return child;
}
exports.execFile = execFile;

exports.fork = function fork () {
  throw new Error("child_process#fork is not currently supported");
};

function serializeEnv (obj) {
  return Object.keys(obj || {}).map(prop => prop + '=' + obj[prop]);
}

function createProcessError (err, options = {}) {
  // If code and signal look OK, this was probably a failure
  // attempting to spawn the process (like ENOENT in node) -- use
  // the code from the error message
  if (!options.code && !options.signal) {
    let match = err.message.match(/(NS_ERROR_\w*)/);
    if (match && match.length > 1)
      err.code = match[1];
    else {
      // If no good error message found, use the passed in exit code;
      // this occurs when killing a process that's already closing,
      // where we want both a valid exit code (0) and the error
      err.code = options.code != null ? options.code : null;
    }
  }
  else
    err.code = options.code != null ? options.code : null;
  err.signal = options.signal || null;
  err.killed = options.killed || false;
  return err;
}
