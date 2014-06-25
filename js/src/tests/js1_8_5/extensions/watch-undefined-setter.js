/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Gary Kwong
 */

var gTestfile = 'watch-undefined-setter.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 560277;
var summary =
  'Crash [@ JSObject::getParent] or [@ js_WrapWatchedSetter] or ' +
  '[@ js_GetClassPrototype]';

this.watch("x", function() { });
Object.defineProperty(this, "x", { set: undefined });

reportCompare(true, true);
