"use strict";
/* exported stuff */

async function timer() {
  return Promise.resolve();
}

function oops() {
  return `oops`;
}

async function inner() {
  oops();
  await timer();
  Promise.resolve().then(async () => {
    Promise.resolve().then(() => {
      oops();
    });
    oops();
  });
  oops();
}

async function stuff() {
  debugger;
  const task = inner();
  oops();
  await task;
  oops();
  debugger;
}
