import { readFile } from "fs/promises";
import { instantiate } from "./temp/userjs-runner.js";
import path from "path";

function aaa(name) {
  return (a, b) => {
    console.log(name, a, b);
  };
}

export async function getCoreModule(path) {
  return await WebAssembly.compile(
    await readFile(new URL(`./temp/${path}`, import.meta.url)),
  );
}

const inst = await instantiate((p) => getCoreModule(p), {
  "local:component/noraneko": {
    setBoolPref: aaa("bool"),
    setIntPref: aaa("int"),
    setStringPref: aaa("string"),
  },
});
var timeInMs = Date.now();
inst.run((await readFile("./user.js")).toString());
console.log(Date.now() - timeInMs);
