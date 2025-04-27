import { componentize } from "@bytecodealliance/componentize-js";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { transpile } from "@bytecodealliance/jco";

console.log("componentizing...");

const { component } = await componentize(await readFile("./comp.js", "utf-8"), {
  witPath: resolve("./user-js.wit"),
  debug: true,
  disableFeatures: ["random", "stdio", "clocks"],
});

//await writeFile("test.component.wasm");
console.log("transpiling...");

const { files } = await transpile(component, {
  name: "userjs-runner",
  instantiation: "async",
  importBindings: "js",
  nodejsCompat: false,
  optArgs: ["--converge", "-Oz"],
});

console.log("saving...");

for (const i in files) {
  await mkdir(dirname("./temp/" + i), { recursive: true });
  await writeFile("./temp/" + i, files[i]);
}
