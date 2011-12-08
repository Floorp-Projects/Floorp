/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

var appendToActual = function(s) {
    actual += s + ',';
}

if (!("gczeal" in this)) {
  gczeal = function() { }
}

