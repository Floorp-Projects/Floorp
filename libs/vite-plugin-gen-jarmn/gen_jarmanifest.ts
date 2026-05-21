// SPDX-License-Identifier: MPL-2.0

export async function generateJarManifest(
  bundle: object,
  options: {
    prefix: string;
    namespace: string;
    register_type: "content" | "skin" | "resource";
    overrides?: string[];
  },
) {
  console.log("generate jar.mn");
  const viteManifest = bundle;

  const arr = [];
  for (const i of Object.values(viteManifest)) {
    const name = (i as { fileName?: string }).fileName;
    if (name) arr.push(name.replaceAll("\\", "/"));
  }
  console.log("generate end jar.mn");

  const header = `noraneko.jar:\n% ${options.register_type} ${options.namespace} %nora-${options.prefix}/ contentaccessible=yes`;
  const overrideLines = (options.overrides ?? [])
    .map((o) => {
      if (o.includes('\n') || o.includes('%')) throw new Error(`Invalid override: ${o}`);
      return `\n% override ${o}`;
    })
    .join("");
  const fileEntries = Array.from(new Set(arr))
    .map((v) => `nora-${options.prefix}/${v} (${v})`)
    .join("\n ");

  return `${header}${overrideLines}\n ${fileEntries}`;
}
