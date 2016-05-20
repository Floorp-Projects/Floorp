/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable spaced-comment */
/* globals StopIteration */

/**
 * This module implements a subset of "Task.js" <http://taskjs.org/>.
 * It is a copy of toolkit/modules/Task.jsm.  Please try not to
 * diverge the API here.
 *
 * Paraphrasing from the Task.js site, tasks make sequential, asynchronous
 * operations simple, using the power of JavaScript's "yield" operator.
 *
 * Tasks are built upon generator functions and promises, documented here:
 *
 * <https://developer.mozilla.org/en/JavaScript/Guide/Iterators_and_Generators>
 * <http://wiki.commonjs.org/wiki/Promises/A>
 *
 * The "Task.spawn" function takes a generator function and starts running it as
 * a task.  Every time the task yields a promise, it waits until the promise is
 * fulfilled.  "Task.spawn" returns a promise that is resolved when the task
 * completes successfully, or is rejected if an exception occurs.
 *
 * -----------------------------------------------------------------------------
 *
 * const {Task} = require("devtools/shared/task");
 *
 * Task.spawn(function* () {
 *
 *   // This is our task. Let's create a promise object, wait on it and capture
 *   // its resolution value.
 *   let myPromise = getPromiseResolvedOnTimeoutWithValue(1000, "Value");
 *   let result = yield myPromise;
 *
 *   // This part is executed only after the promise above is fulfilled (after
 *   // one second, in this imaginary example).  We can easily loop while
 *   // calling asynchronous functions, and wait multiple times.
 *   for (let i = 0; i < 3; i++) {
 *     result += yield getPromiseResolvedOnTimeoutWithValue(50, "!");
 *   }
 *
 *   return "Resolution result for the task: " + result;
 * }).then(function (result) {
 *
 *   // result == "Resolution result for the task: Value!!!"
 *
 *   // The result is undefined if no value was returned.
 *
 * }, function (exception) {
 *
 *   // Failure!  We can inspect or report the exception.
 *
 * });
 *
 * -----------------------------------------------------------------------------
 *
 * This module implements only the "Task.js" interfaces described above, with no
 * additional features to control the task externally, or do custom scheduling.
 * It also provides the following extensions that simplify task usage in the
 * most common cases:
 *
 * - The "Task.spawn" function also accepts an iterator returned by a generator
 *   function, in addition to a generator function.  This way, you can call into
 *   the generator function with the parameters you want, and with "this" bound
 *   to the correct value.  Also, "this" is never bound to the task object when
 *   "Task.spawn" calls the generator function.
 *
 * - In addition to a promise object, a task can yield the iterator returned by
 *   a generator function.  The iterator is turned into a task automatically.
 *   This reduces the syntax overhead of calling "Task.spawn" explicitly when
 *   you want to recurse into other task functions.
 *
 * - The "Task.spawn" function also accepts a primitive value, or a function
 *   returning a primitive value, and treats the value as the result of the
 *   task.  This makes it possible to call an externally provided function and
 *   spawn a task from it, regardless of whether it is an asynchronous generator
 *   or a synchronous function.  This comes in handy when iterating over
 *   function lists where some items have been converted to tasks and some not.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Promise = require("promise");

// The following error types are considered programmer errors, which should be
// reported (possibly redundantly) so as to let programmers fix their code.
const ERRORS_TO_REPORT = ["EvalError", "RangeError", "ReferenceError",
                          "TypeError"];

/**
 * The Task currently being executed
 */
var gCurrentTask = null;

/**
 * If `true`, capture stacks whenever entering a Task and rewrite the
 * stack any exception thrown through a Task.
 */
var gMaintainStack = false;

/**
 * Iterate through the lines of a string.
 *
 * @return Iterator<string>
 */
function* linesOf(string) {
  let reLine = /([^\r\n])+/g;
  let match;
  while ((match = reLine.exec(string))) {
    yield [match[0], match.index];
  }
}

/**
 * Detect whether a value is a generator.
 *
 * @param aValue
 *        The value to identify.
 * @return A boolean indicating whether the value is a generator.
 */
function isGenerator(value) {
  return Object.prototype.toString.call(value) == "[object Generator]";
}

////////////////////////////////////////////////////////////////////////////////
//// Task

/**
 * This object provides the public module functions.
 */
this.Task = {
  /**
   * Creates and starts a new task.
   *
   * @param task
   *        - If you specify a generator function, it is called with no
   *          arguments to retrieve the associated iterator.  The generator
   *          function is a task, that is can yield promise objects to wait
   *          upon.
   *        - If you specify the iterator returned by a generator function you
   *          called, the generator function is also executed as a task.  This
   *          allows you to call the function with arguments.
   *        - If you specify a function that is not a generator, it is called
   *          with no arguments, and its return value is used to resolve the
   *          returned promise.
   *        - If you specify anything else, you get a promise that is already
   *          resolved with the specified value.
   *
   * @return A promise object where you can register completion callbacks to be
   *         called when the task terminates.
   */
  spawn: function (task) {
    return createAsyncFunction(task).call(undefined);
  },

  /**
   * Create and return an 'async function' that starts a new task.
   *
   * This is similar to 'spawn' except that it doesn't immediately start
   * the task, it binds the task to the async function's 'this' object and
   * arguments, and it requires the task to be a function.
   *
   * It simplifies the common pattern of implementing a method via a task,
   * like this simple object with a 'greet' method that has a 'name' parameter
   * and spawns a task to send a greeting and return its reply:
   *
   * let greeter = {
   *   message: "Hello, NAME!",
   *   greet: function(name) {
   *     return Task.spawn((function* () {
   *       return yield sendGreeting(this.message.replace(/NAME/, name));
   *     }).bind(this);
   *   })
   * };
   *
   * With Task.async, the method can be declared succinctly:
   *
   * let greeter = {
   *   message: "Hello, NAME!",
   *   greet: Task.async(function* (name) {
   *     return yield sendGreeting(this.message.replace(/NAME/, name));
   *   })
   * };
   *
   * While maintaining identical semantics:
   *
   * greeter.greet("Mitchell").then((reply) => { ... }); // behaves the same
   *
   * @param task
   *        The task function to start.
   *
   * @return A function that starts the task function and returns its promise.
   */
  async: function (task) {
    if (typeof (task) != "function") {
      throw new TypeError("task argument must be a function");
    }

    return createAsyncFunction(task);
  },

  /**
   * Constructs a special exception that, when thrown inside a legacy generator
   * function (non-star generator), allows the associated task to be resolved
   * with a specific value.
   *
   * Example: throw new Task.Result("Value");
   */
  Result: function (value) {
    this.value = value;
  }
};

function createAsyncFunction(task) {
  let asyncFunction = function () {
    let result = task;
    if (task && typeof (task) == "function") {
      if (task.isAsyncFunction) {
        throw new TypeError(
          "Cannot use an async function in place of a promise. " +
          "You should either invoke the async function first " +
          "or use 'Task.spawn' instead of 'Task.async' to start " +
          "the Task and return its promise.");
      }

      try {
        // Let's call into the function ourselves.
        result = task.apply(this, arguments);
      } catch (ex) {
        if (ex instanceof Task.Result) {
          return Promise.resolve(ex.value);
        }
        return Promise.reject(ex);
      }
    }

    if (isGenerator(result)) {
      // This is an iterator resulting from calling a generator function.
      return new TaskImpl(result).deferred.promise;
    }

    // Just propagate the given value to the caller as a resolved promise.
    return Promise.resolve(result);
  };

  asyncFunction.isAsyncFunction = true;

  return asyncFunction;
}

////////////////////////////////////////////////////////////////////////////////
//// TaskImpl

/**
 * Executes the specified iterator as a task, and gives access to the promise
 * that is fulfilled when the task terminates.
 */
function TaskImpl(iterator) {
  if (gMaintainStack) {
    this._stack = (new Error()).stack;
  }
  this.deferred = Promise.defer();
  this._iterator = iterator;
  this._isStarGenerator = !("send" in iterator);
  this._run(true);
}

TaskImpl.prototype = {
  /**
   * Includes the promise object where task completion callbacks are registered,
   * and methods to resolve or reject the promise at task completion.
   */
  deferred: null,

  /**
   * The iterator returned by the generator function associated with this task.
   */
  _iterator: null,

  /**
   * Whether this Task is using a star generator.
   */
  _isStarGenerator: false,

  /**
   * Main execution routine, that calls into the generator function.
   *
   * @param sendResolved
   *        If true, indicates that we should continue into the generator
   *        function regularly (if we were waiting on a promise, it was
   *        resolved). If true, indicates that we should cause an exception to
   *        be thrown into the generator function (if we were waiting on a
   *        promise, it was rejected).
   * @param sendValue
   *        Resolution result or rejection exception, if any.
   */
  _run: function (sendResolved, sendValue) {
    try {
      gCurrentTask = this;

      if (this._isStarGenerator) {
        try {
          let result = sendResolved ? this._iterator.next(sendValue)
                                    : this._iterator.throw(sendValue);

          if (result.done) {
            // The generator function returned.
            this.deferred.resolve(result.value);
          } else {
            // The generator function yielded.
            this._handleResultValue(result.value);
          }
        } catch (ex) {
          // The generator function failed with an uncaught exception.
          this._handleException(ex);
        }
      } else {
        try {
          let yielded = sendResolved ? this._iterator.send(sendValue)
                                     : this._iterator.throw(sendValue);
          this._handleResultValue(yielded);
        } catch (ex) {
          if (ex instanceof Task.Result) {
            // The generator function threw the special exception that
            // allows it to return a specific value on resolution.
            this.deferred.resolve(ex.value);
          } else if (ex instanceof StopIteration) {
            // The generator function terminated with no specific result.
            this.deferred.resolve(undefined);
          } else {
            // The generator function failed with an uncaught exception.
            this._handleException(ex);
          }
        }
      }
    } finally {
      //
      // At this stage, the Task may have finished executing, or have
      // walked through a `yield` or passed control to a sub-Task.
      // Regardless, if we still own `gCurrentTask`, reset it. If we
      // have not finished execution of this Task, re-entering `_run`
      // will set `gCurrentTask` to `this` as needed.
      //
      // We just need to be careful here in case we hit the following
      // pattern:
      //
      //   Task.spawn(foo);
      //   Task.spawn(bar);
      //
      // Here, `foo` and `bar` may be interleaved, so when we finish
      // executing `foo`, `gCurrentTask` may actually either `foo` or
      // `bar`. If `gCurrentTask` has already been set to `bar`, leave
      // it be and it will be reset to `null` once `bar` is complete.
      //
      if (gCurrentTask == this) {
        gCurrentTask = null;
      }
    }
  },

  /**
   * Handle a value yielded by a generator.
   *
   * @param value
   *        The yielded value to handle.
   */
  _handleResultValue: function (value) {
    // If our task yielded an iterator resulting from calling another
    // generator function, automatically spawn a task from it, effectively
    // turning it into a promise that is fulfilled on task completion.
    if (isGenerator(value)) {
      value = Task.spawn(value);
    }

    if (value && typeof (value.then) == "function") {
      // We have a promise object now. When fulfilled, call again into this
      // function to continue the task, with either a resolution or rejection
      // condition.
      value.then(this._run.bind(this, true),
                  this._run.bind(this, false));
    } else {
      // If our task yielded a value that is not a promise, just continue and
      // pass it directly as the result of the yield statement.
      this._run(true, value);
    }
  },

  /**
   * Handle an uncaught exception thrown from a generator.
   *
   * @param exception
   *        The uncaught exception to handle.
   */
  _handleException: function (exception) {
    gCurrentTask = this;

    if (exception && typeof exception == "object" && "stack" in exception) {
      let stack = exception.stack;

      if (gMaintainStack &&
          exception._capturedTaskStack != this._stack &&
          typeof stack == "string") {
        // Rewrite the stack for more readability.

        let bottomStack = this._stack;

        stack = Task.Debugging.generateReadableStack(stack);

        exception.stack = stack;

        // If exception is reinjected in the same task and rethrown,
        // we don't want to perform the rewrite again.
        exception._capturedTaskStack = bottomStack;
      } else if (!stack) {
        stack = "Not available";
      }

      if ("name" in exception &&
          ERRORS_TO_REPORT.indexOf(exception.name) != -1) {
        // We suspect that the exception is a programmer error, so we now
        // display it using dump().  Note that we do not use Cu.reportError as
        // we assume that this is a programming error, so we do not want end
        // users to see it. Also, if the programmer handles errors correctly,
        // they will either treat the error or log them somewhere.

        dump("*************************\n");
        dump("A coding exception was thrown and uncaught in a Task.\n\n");
        dump("Full message: " + exception + "\n");
        dump("Full stack: " + exception.stack + "\n");
        dump("*************************\n");
      }
    }

    this.deferred.reject(exception);
  },

  get callerStack() {
    // Cut `this._stack` at the last line of the first block that
    // contains task.js, keep the tail.
    for (let [line, index] of linesOf(this._stack || "")) {
      if (line.indexOf("/task.js:") == -1) {
        return this._stack.substring(index);
      }
    }
    return "";
  }
};

Task.Debugging = {

  /**
   * Control stack rewriting.
   *
   * If `true`, any exception thrown from a Task will be rewritten to
   * provide a human-readable stack trace. Otherwise, stack traces will
   * be left unchanged.
   *
   * There is a (small but existing) runtime cost associated to stack
   * rewriting, so you should probably not activate this in production
   * code.
   *
   * @type {bool}
   */
  get maintainStack() {
    return gMaintainStack;
  },
  set maintainStack(x) {
    if (!x) {
      gCurrentTask = null;
    }
    gMaintainStack = x;
    return x;
  },

  /**
   * Generate a human-readable stack for an error raised in
   * a Task.
   *
   * @param {string} topStack The stack provided by the error.
   * @param {string=} prefix Optionally, a prefix for each line.
   */
  generateReadableStack: function (topStack, prefix = "") {
    if (!gCurrentTask) {
      return topStack;
    }

    // Cut `topStack` at the first line that contains task.js, keep the head.
    let lines = [];
    for (let [line] of linesOf(topStack)) {
      if (line.indexOf("/task.js:") != -1) {
        break;
      }
      lines.push(prefix + line);
    }
    if (!prefix) {
      lines.push(gCurrentTask.callerStack);
    } else {
      for (let [line] of linesOf(gCurrentTask.callerStack)) {
        lines.push(prefix + line);
      }
    }

    return lines.join("\n");
  }
};

exports.Task = Task;
