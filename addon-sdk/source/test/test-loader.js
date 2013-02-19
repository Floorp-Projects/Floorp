/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

let { Loader, main, unload, parseStack } = require('toolkit/loader');

let root = module.uri.substr(0, module.uri.lastIndexOf('/'))

exports['test dependency cycles'] = function(assert) {
  let uri = root + '/fixtures/loader/cycles/';
  let loader = Loader({ paths: { '': uri } });

  let program = main(loader, 'main');

  assert.equal(program.a.b, program.b, 'module `a` gets correct `b`');
  assert.equal(program.b.a, program.a, 'module `b` gets correct `a`');
  assert.equal(program.c.main, program, 'module `c` gets correct `main`');

  unload(loader);
}

exports['test syntax errors'] = function(assert) {
  let uri = root + '/fixtures/loader/syntax-error/';
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main');
  } catch (error) {
    assert.equal(error.name, "SyntaxError", "throws syntax error");
    assert.equal(error.fileName.split("/").pop(), "error.js",
              "Error contains filename");
    assert.equal(error.lineNumber, 7, "error is on line 7")
    let stack = parseStack(error.stack);
    assert.equal(stack.pop().fileName, uri + "main.js",
                 "loader stack is omitted");
    assert.equal(stack.pop().fileName, module.uri,
                 "previous in the stack is test module");

  } finally {
    unload(loader);
  }
}

exports['test missing module'] = function(assert) {
  let uri = root + '/fixtures/loader/missing/'
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(error.message, "Module `not-found` is not found at " +
                uri + "not-found.js", "throws if error not found");

    assert.equal(error.fileName.split("/").pop(), "main.js",
                 "Error fileName is requirer module");

    assert.equal(error.lineNumber, 7, "error is on line 7");

    let stack = parseStack(error.stack);

    assert.equal(stack.pop().fileName, uri + "main.js",
                 "loader stack is omitted");

    assert.equal(stack.pop().fileName, module.uri,
                 "previous in the stack is test module");
  } finally {
    unload(loader);
  }
}

exports['test exceptions in modules'] = function(assert) {
  let uri = root + '/fixtures/loader/exceptions/'

  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(error.message, "Boom!", "thrown errors propagate");

    assert.equal(error.fileName.split("/").pop(), "boomer.js",
                 "Error comes from the module that threw it");

    assert.equal(error.lineNumber, 8, "error is on line 8");

    let stack = parseStack(error.stack);

    let frame = stack.pop()
    assert.equal(frame.fileName, uri + "boomer.js",
                 "module that threw is first in the stack");
    assert.equal(frame.name, "exports.boom",
                 "name is in the stack");

    frame = stack.pop()
    assert.equal(frame.fileName, uri + "main.js",
                 "module that called it is next in the stack");
    assert.equal(frame.lineNumber, 9, "caller line is in the stack");


    assert.equal(stack.pop().fileName, module.uri,
                 "this test module is next in the stack");
  } finally {
    unload(loader);
  }
}

exports['test early errors in module'] = function(assert) {
  let uri = root + '/fixtures/loader/errors/';
  let loader = Loader({ paths: { '': uri } });

  try {
    let program = main(loader, 'main')
  } catch (error) {
    assert.equal(String(error),
                 "Error: opening input stream (invalid filename?)",
                 "thrown errors propagate");

    assert.equal(error.fileName.split("/").pop(), "boomer.js",
                 "Error comes from the module that threw it");

    assert.equal(error.lineNumber, 7, "error is on line 7");

    let stack = parseStack(error.stack);

    let frame = stack.pop()
    assert.equal(frame.fileName, uri + "boomer.js",
                 "module that threw is first in the stack");

    frame = stack.pop()
    assert.equal(frame.fileName, uri + "main.js",
                 "module that called it is next in the stack");
    assert.equal(frame.lineNumber, 7, "caller line is in the stack");


    assert.equal(stack.pop().fileName, module.uri,
                 "this test module is next in the stack");
  } finally {
    unload(loader);
  }
}

require('test').run(exports);
