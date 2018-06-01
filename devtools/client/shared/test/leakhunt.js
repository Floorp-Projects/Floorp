/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Memory leak hunter. Walks a tree of objects looking for DOM nodes.
 * Usage:
 * leakHunt({
 *   thing: thing,
 *   otherthing: otherthing
 * });
 */
function leakHunt(root) {
  const path = [];
  const seen = [];

  try {
    const output = leakHunt.inner(root, path, seen);
    output.forEach(function(line) {
      dump(line + "\n");
    });
  } catch (ex) {
    dump(ex + "\n");
  }
}

leakHunt.inner = function(root, path, seen) {
  const prefix = new Array(path.length).join("  ");

  const reply = [];
  function log(msg) {
    reply.push(msg);
  }

  let direct;
  try {
    direct = Object.keys(root);
  } catch (ex) {
    log(prefix + "  Error enumerating: " + ex);
    return reply;
  }

  try {
    let index = 0;
    for (const data of root) {
      const prop = "" + index;
      leakHunt.digProperty(prop, data, path, seen, direct, log);
      index++;
    }
  } catch (ex) {
    /* Ignore things that are not enumerable */
  }

  for (const prop in root) {
    let data;
    try {
      data = root[prop];
    } catch (ex) {
      log(prefix + "  " + prop + " = Error: " + ex.toString().substring(0, 30));
      continue;
    }

    leakHunt.digProperty(prop, data, path, seen, direct, log);
  }

  return reply;
};

leakHunt.hide = [ /^string$/, /^number$/, /^boolean$/, /^null/, /^undefined/ ];

leakHunt.noRecurse = [
  /^string$/, /^number$/, /^boolean$/, /^null/, /^undefined/,
  /^Window$/, /^Document$/,
  /^XULDocument$/, /^XULElement$/,
  /^DOMWindow$/, /^HTMLDocument$/, /^HTML.*Element$/, /^ChromeWindow$/
];

leakHunt.digProperty = function(prop, data, path, seen, direct, log) {
  const newPath = path.slice();
  newPath.push(prop);
  const prefix = new Array(newPath.length).join("  ");

  let recurse = true;
  let message = leakHunt.getType(data);

  if (leakHunt.matchesAnyPattern(message, leakHunt.hide)) {
    return;
  }

  if (message === "function" && !direct.includes(prop)) {
    return;
  }

  if (message === "string") {
    const extra = data.length > 10 ? data.substring(0, 9) + "_" : data;
    message += ' "' + extra.replace(/\n/g, "|") + '"';
    recurse = false;
  } else if (leakHunt.matchesAnyPattern(message, leakHunt.noRecurse)) {
    message += " (no recurse)";
    recurse = false;
  } else if (seen.includes(data)) {
    message += " (already seen)";
    recurse = false;
  }

  if (recurse) {
    seen.push(data);
    const lines = leakHunt.inner(data, newPath, seen);
    if (lines.length == 0) {
      if (message !== "function") {
        log(prefix + prop + " = " + message + " { }");
      }
    } else {
      log(prefix + prop + " = " + message + " {");
      lines.forEach(function(line) {
        log(line);
      });
      log(prefix + "}");
    }
  } else {
    log(prefix + prop + " = " + message);
  }
};

leakHunt.matchesAnyPattern = function(str, patterns) {
  let match = false;
  patterns.forEach(function(pattern) {
    if (str.match(pattern)) {
      match = true;
    }
  });
  return match;
};

leakHunt.getType = function(data) {
  if (data === null) {
    return "null";
  }
  if (data === undefined) {
    return "undefined";
  }

  let type = typeof data;
  if (type === "object" || type === "Object") {
    type = leakHunt.getCtorName(data);
  }

  return type;
};

leakHunt.getCtorName = function(obj) {
  try {
    if (obj.constructor && obj.constructor.name) {
      return obj.constructor.name;
    }
  } catch (ex) {
    return "UnknownObject";
  }

  // If that fails, use Objects toString which sometimes gives something
  // better than 'Object', and at least defaults to Object if nothing better
  return Object.prototype.toString.call(obj).slice(8, -1);
};
