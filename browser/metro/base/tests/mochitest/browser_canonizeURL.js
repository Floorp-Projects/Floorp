/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  let testcases = [
    ["example", {}, "example"],
    ["example", {ctrlKey: true}, "http://www.example.com/"],
    ["example.org", {ctrlKey: true}, "example.org"],
    ["example", {shiftKey: true}, "http://www.example.net/"],
    ["example", {shiftKey: true, ctrlKey: true}, "http://www.example.org/"],
    ["  example  ", {ctrlKey: true}, "http://www.example.com/"],
    [" example/a ", {ctrlKey: true}, "http://www.example.com/a"]
  ];
  for (let [input, modifiers, result] of testcases) {
    is(BrowserUI._edit._canonizeURL(input, modifiers), result, input + " -> " + result);
  }
}
