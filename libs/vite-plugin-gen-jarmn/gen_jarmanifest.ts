// SPDX-License-Identifier: MPL-2.0

export async function generateJarManifest(
  bundle: object,
  options: {
    prefix: string;
    namespace: string;
    register_type: "content" | "skin" | "resource";
  },
) {
  console.log("generate jar.mn");
  const viteManifest = bundle;

  const arr = [];
  for (const i of Object.values(viteManifest)) {
    arr.push((i as { fileName: string })["fileName"].replaceAll("\\", "/"));
  }
  // Vite emits the HTML entry point outside of the Rollup bundle.
  arr.push("index.html");

  console.log("generate end jar.mn");

  return `noraneko.jar:\n% ${options.register_type} ${options.namespace} %nora-${options.prefix}/ contentaccessible=yes\n ${Array.from(
    new Set(arr),
  )
    .map((v) => `nora-${options.prefix}/${v} (${v})`)
    .join("\n ")}`;
}
