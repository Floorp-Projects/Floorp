// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

try {
  (function() {
    __defineSetter__("x", Math.sin);
  } ());
  function::x =
    Proxy.createFunction(function() {
        return {
          get: function() {
            return [];
          }
        };
      } (),
      function() {});
} catch(e) {}

reportCompare(0, 0, "ok");
