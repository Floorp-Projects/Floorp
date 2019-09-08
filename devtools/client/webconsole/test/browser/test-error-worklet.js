/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function throw_process() {
  throw "process"; // eslint-disable-line no-throw-literal
}

class ErrorProcessor extends AudioWorkletProcessor {
  process() {
    throw_process();
  }
}
registerProcessor("error", ErrorProcessor);

function throw_error() {
  throw new Error("addModule");
}

throw_error();
