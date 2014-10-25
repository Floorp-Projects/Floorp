/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Utilities for testing SVG matrices
 */

function createMatrix(a, b, c, d, e, f)
{
  var svg = document.getElementsByTagName("svg")[0];
  var m = svg.createSVGMatrix();
  m.a = a;
  m.b = b;
  m.c = c;
  m.d = d;
  m.e = e;
  m.f = f;
  return m;
}

// Lightweight dummy Matrix class for representing arrays that get passed in
function MatrixFromArray(a)
{
  this.a = a[0];
  this.b = a[1];
  this.c = a[2];
  this.d = a[3];
  this.e = a[4];
  this.f = a[5];
}

function cmpMatrix(a, b, msg)
{
  if (a.constructor === Array)
    a = new MatrixFromArray(a);
  if (b.constructor === Array)
    b = new MatrixFromArray(b);

  ok(a.a == b.a &&
     a.b == b.b &&
     a.c == b.c &&
     a.d == b.d &&
     a.e == b.e &&
     a.f == b.f,
     msg + " - got " + formatMatrix(a)
         + ", expected " + formatMatrix(b));
}

function roughCmpMatrix(a, b, msg)
{
  if (a.constructor === Array)
    a = new MatrixFromArray(a);
  if (b.constructor === Array)
    b = new MatrixFromArray(b);

  const tolerance = 1 / 65535;
  ok(Math.abs(b.a - a.a) < tolerance &&
     Math.abs(b.b - a.b) < tolerance &&
     Math.abs(b.c - a.c) < tolerance &&
     Math.abs(b.d - a.d) < tolerance &&
     Math.abs(b.e - a.e) < tolerance &&
     Math.abs(b.f - a.f) < tolerance,
     msg + " - got " + formatMatrix(a)
         + ", expected " + formatMatrix(b));
}

function formatMatrix(m)
{
  if (m.constructor != Array)
    return "(" + [m.a, m.b, m.c, m.d, m.e, m.f].join(', ') + ")";

  return "(" + m.join(', ') + ")";
}
