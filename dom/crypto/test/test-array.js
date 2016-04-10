/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var MOCHITEST = false;

function Test(name, test) {
  this.name = name;
  this.startTime = null;
  this.endTime = null;
  this.result = null;
  this.row = null;

  this.run = function() {
    // Note the start time
    this.startTime = new Date();
    // Run the test
    try {
      test.call(this);
    } catch (e) {
      console.log(e);
      console.log(e.stack);
      this.complete(false);
    }
  };

  this.memcmp_complete = function(x, y) {
    var passfail = util.memcmp(x, y);
    if (!passfail) {
      console.log("expected: " + util.abv2hex(x));
      console.log("   got: " + util.abv2hex(y));
    }
    this.complete(passfail);
  };

  this.complete = function(result) {
    if (MOCHITEST) { ok(result, this.name); }

    // Note the end time
    this.endTime = new Date();
    // Set result
    this.result = result;
    // Re-draw the row
    this.draw();
    this.next();
  };

  this.next = function() {
    if (this.oncomplete) {
      this.oncomplete();
    }
  };

  this.setRow = function(id) {
    this.row = document.getElementById(id).getElementsByTagName("td");
  };

  this.draw = function() {
    if (!this.row) return;

    // Print the name of the test
    if (this.name) {
      this.row[0].innerHTML = this.name;
      var that = this;
      this.row[0].onclick = function() { that.run(); }
    } else {
      this.row[0] = "";
    }

    // Print the result of the test
    if (this.result == true) {
      this.row[1].className = "pass";
      this.row[1].innerHTML = "PASS";
    } else if (this.result == false) {
      this.row[1].className = "fail";
      this.row[1].innerHTML = "FAIL";
    } else {
      //this.row[1].innerHTML = "";
      this.row[1].innerHTML = this.result;
    }

    // Print the elapsed time, if known
    if (this.startTime &&  this.endTime) {
      this.row[2].innerHTML = (this.endTime - this.startTime) + " ms";
    } else {
      this.row[2].innerHTML = "";
    }
  };
}

function WorkerTest(worker, name, test) {
  this.name = `${name} (Worker)`;
  this.startTime = null;
  this.endTime = null;
  this.result = null;
  this.row = null;

  this.run = function() {
    // Note the start time
    this.startTime = new Date();

    // Send the test code to the worker.
    worker.postMessage(test.toSource());

    // We expect only boolean responses from the worker script.
    worker.onmessage = e => this.complete(e.data);
    worker.onerror = e => this.complete(false);
  };

  var base = new Test(name, test);

  // Inherit what we need from the |Test| class. We can't simply use its
  // prototype as Test is just a function that can be used like a constructor.
  for (var method of ["draw", "setRow", "next", "complete"]) {
    this[method] = base[method].bind(this);
  }
}

var TestArray = {
  tests: [],
  table: null,
  passSpan: null,
  failSpan: null,
  pendingSpan: null,
  pass: 0,
  fail: 0,
  pending: 0,
  currTest: 0,
  worker: new Worker("test-worker.js"),

  addTest: function(name, testFn) {
    // Give it a reference to the array
    var test = new Test(name, testFn);
    test.ta = this;

    // Add test to tests
    this.tests.push(test);

    // Run the test in a worker too.
    this.tests.push(new WorkerTest(this.worker, name, testFn));
  },

  updateSummary: function() {
    this.pass = this.fail = this.pending = 0;
    for (var i=0; i<this.tests.length; ++i) {
      if (this.tests[i].result == true)  this.pass++;
      if (this.tests[i].result == false) this.fail++;
      if (this.tests[i].result == null)  this.pending++;
    }
    this.passSpan.innerHTML = this.pass;
    this.failSpan.innerHTML = this.fail;
    this.pendingSpan.innerHTML = this.pending;
  },

  load: function() {
    // Grab reference to table and summary numbers
    this.table = document.getElementById("results");
    this.passSpan = document.getElementById("passN");
    this.failSpan = document.getElementById("failN");
    this.pendingSpan = document.getElementById("pendingN");

    // Populate everything initially
    this.updateSummary();
    for (var i=0; i<this.tests.length; ++i) {
      var tr = document.createElement("tr");
      tr.id = "test" + i;
      tr.appendChild(document.createElement("td"));
      tr.appendChild(document.createElement("td"));
      tr.appendChild(document.createElement("td"));
      this.table.appendChild(tr);
      this.tests[i].setRow(tr.id);
      this.tests[i].draw();
    }
  },

  run: function() {
    this.currTest = 0;
    this.runNextTest();
  },

  runNextTest: function() {
    this.updateSummary();
    var i = this.currTest++;
    if (i >= this.tests.length) {
      if (MOCHITEST) { SimpleTest.finish(); }
      return;
    }

    var self = this;
    this.tests[i].oncomplete = function() {
      self.runNextTest();
    }
    this.tests[i].run();
  }
}

if (window.addEventListener) {
  window.addEventListener("load", function() { TestArray.load(); } );
} else {
  window.attachEvent("onload", function() { TestArray.load(); } );
}

function start() {
  TestArray.run();
}

MOCHITEST = ("SimpleTest" in window);
if (MOCHITEST) {
  SimpleTest.waitForExplicitFinish();
  SimpleTest.requestLongerTimeout(2);
  window.addEventListener("load", function() {
    SimpleTest.waitForFocus(start);
  });
}

function error(test) {
  return function(x) {
    console.log("ERROR :: " + x);
    test.complete(false);
    throw x;
  }
}

function complete(test, valid) {
  return function(x) {
    console.log("COMPLETE")
    console.log(x);
    if (valid) {
      test.complete(valid(x));
    } else {
      test.complete(true);
    }
  }
}

function memcmp_complete(test, value) {
  return function(x) {
    console.log("COMPLETE")
    console.log(x);
    test.memcmp_complete(value, x);
  }
}
