/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Memory leak hunter. Walks a tree of objects looking for DOM nodes.
 * Usage:
 * leakHunt({
 *   thing: thing,
 *   otherthing: otherthing
 * });
 */

var noRecurse = [
  /^string$/, /^number$/, /^boolean$/, /^null/, /^undefined/,
  /^Window$/, /^Document$/,
  /^XULDocument$/, /^XULElement$/,
  /^DOMWindow$/, /^HTMLDocument$/, /^HTML.*Element$/
];

var hide = [ /^string$/, /^number$/, /^boolean$/, /^null/, /^undefined/ ];

function leakHunt(root, path, seen) {
  path = path || [];
  seen = seen || [];

  try {
    var output = leakHuntInner(root, path, seen);
    output.forEach(function(line) {
      dump(line + '\n');
    });
  }
  catch (ex) {
    dump(ex + '\n');
  }
}

function leakHuntInner(root, path, seen) {
  var prefix = new Array(path.length).join('  ');

  var reply = [];
  function log(msg) {
    reply.push(msg);
  }

  var direct
  try {
    direct = Object.keys(root);
  }
  catch (ex) {
    log(prefix + '  Error enumerating: ' + ex);
    return reply;
  }

  for (var prop in root) {
    var newPath = path.slice();
    newPath.push(prop);
    prefix = new Array(newPath.length).join('  ');

    var data;
    try {
      data = root[prop];
    }
    catch (ex) {
      log(prefix + prop + '  Error reading: ' + ex);
      continue;
    }

    var recurse = true;
    var message = getType(data);

    if (matchesAnyPattern(message, hide)) {
      continue;
    }

    if (message === 'function' && direct.indexOf(prop) == -1) {
      continue;
    }

    if (message === 'string') {
      var extra = data.length > 10 ? data.substring(0, 9) + '_' : data;
      message += ' "' + extra.replace(/\n/g, "|") + '"';
      recurse = false;
    }
    else if (matchesAnyPattern(message, noRecurse)) {
      message += ' (no recurse)'
      recurse = false;
    }
    else if (seen.indexOf(data) !== -1) {
      message += ' (already seen)';
      recurse = false;
    }

    if (recurse) {
      seen.push(data);
      var lines = leakHuntInner(data, newPath, seen);
      if (lines.length == 0) {
        if (message !== 'function') {
          log(prefix + prop + ' = ' + message + ' { }');
        }
      }
      else {
        log(prefix + prop + ' = ' + message + ' {');
        lines.forEach(function(line) {
          reply.push(line);
        });
        log(prefix + '}');
      }
    }
    else {
      log(prefix + prop + ' = ' + message);
    }
  }

  return reply;
}

function matchesAnyPattern(str, patterns) {
  var match = false;
  patterns.forEach(function(pattern) {
    if (str.match(pattern)) {
      match = true;
    }
  });
  return match;
}

function getType(data) {
  if (data === null) {
    return 'null';
  }
  if (data === undefined) {
    return 'undefined';
  }

  var type = typeof data;
  if (type === 'object' || type === 'Object') {
    type = getCtorName(data);
  }

  return type;
}

function getCtorName(aObj) {
  try {
    if (aObj.constructor && aObj.constructor.name) {
      return aObj.constructor.name;
    }
  }
  catch (ex) {
    return 'UnknownObject';
  }

  // If that fails, use Objects toString which sometimes gives something
  // better than 'Object', and at least defaults to Object if nothing better
  return Object.prototype.toString.call(aObj).slice(8, -1);
}
