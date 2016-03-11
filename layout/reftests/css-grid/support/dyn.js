/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function loadURL(url,callback) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (xhttp.readyState == 4 && xhttp.status == 200) {
      callback(xhttp.responseText);
    }
  };
  xhttp.open("GET", url, true);
  xhttp.send();
}

function dyn1(selector) {
  // get an array of elements matching |selector|
  var elems = Array.prototype.slice.call(document.querySelectorAll(selector))

  // remove the first item in each grid
  var removed = elems.map(function(e) {
    var child = e.children[0];
    if (child) {
      var next = child.nextSibling;
      e.removeChild(child);
      return [ e, child, next ];
    } else {
      return null;
    }
  });

  document.body.style.display = 'block';
  document.body.offsetHeight;

  // insert the removed item
  removed.map(function(a) {
    if (a) {
      a[0].insertBefore(a[1],a[2]);
    }
  });
}

function dyn2(selector) {
  // get an array of elements matching |selector|
  var elems = Array.prototype.slice.call(document.querySelectorAll(selector))

  // inject a new first item in each grid
  var inserted = elems.map(function(e) {
    var child = document.createElement('span');
    e.insertBefore(child, e.firstChild);
    return [ e, child ];
  });

  document.body.style.display = 'block';
  document.body.offsetHeight;

  // remove the inserted item
  inserted.map(function(a) {
    a[0].removeChild(a[1]);
  });
}

function dyn3(selector) {
  // get an array of elements matching |selector|
  var elems = Array.prototype.slice.call(document.querySelectorAll(selector))

  // remove the second item in each grid
  var removed = elems.map(function(e) {
    var child = e.children[1];
    if (child) {
      var next = child.nextSibling;
      e.removeChild(child);
      return [ e, child, next ];
    } else {
      return null;
    }
  });

  document.body.style.display = 'block';
  document.body.offsetHeight;

  // insert the removed items
  removed.map(function(a) {
    if (a) {
      a[0].insertBefore(a[1],a[2]);
    }
  });
}

function dyn4(selector) {
  dyn3(selector);
  dyn2(selector);
}

function dyn5(selector) {
  // get an array of elements matching |selector|
  var elems = Array.prototype.slice.call(document.querySelectorAll(selector))

  // inject 20 new items in each grid
  var inserted = elems.map(function(e) {
    var a = new Array;
    for (var i = 0; i < 20; ++i) {
      var child = document.createElement('span');
      e.insertBefore(child, e.firstChild);
      a.push(child);
    }
    return [ e, a ];
  });

  document.body.style.display = 'block';
  document.body.offsetHeight;

  // remove the inserted item
  inserted.map(function(a) {
    a[1].forEach(function(child) {
      a[0].removeChild(child);
    });
  });
}

function dynamicTest(url, callback) {
  document.body.style.display='';
  document.body.offsetHeight;
  loadURL(url,callback);
}
