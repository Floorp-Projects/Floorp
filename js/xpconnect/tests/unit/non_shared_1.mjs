import { setGlobal } from "./non_shared_2.mjs";

globalThis["loaded"].push(1);

globalThis["counter"] = 0;

let counter = 0;

export function getCounter() {
  return counter;
}

export function incCounter() {
  counter++;
}

export function putCounter() {
  setGlobal("counter", counter);
}
