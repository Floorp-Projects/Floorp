"use strict";
import { imported } from "somewhere";
import { getter } from "somewhere-else";

const blackbox = x => [x].pop();

function firstCall() {
  const value = 42;
  const temp = imported;
  const temp2 = getter;
  const localWithGetter = {
    get value() { return blackbox(Promise.resolve()); }
  };
  const unmapped = 100;
  debugger;
}
