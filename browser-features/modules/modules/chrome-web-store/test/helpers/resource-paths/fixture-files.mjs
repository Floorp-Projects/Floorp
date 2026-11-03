// SPDX-License-Identifier: MPL-2.0
import { controlProgram, pathProgram, sidebarProgram } from "./programs.mjs";
/** @param {string} kind @param {boolean} compatible @param {boolean} native @param {string} originalId @returns {Record<string,string>} */
export function resourceFiles(kind, compatible, native, originalId) {
  const id = compatible
    ? originalId + "@chromium-extension"
    : "ordinary-path-" + originalId + "@example.invalid";
  const manifest = {
    manifest_version: 3,
    name: "Owned resource " + kind,
    version: "1",
    permissions: ["scripting", "tabs", "storage"],
    host_permissions: ["http://127.0.0.1/*"],
    ...(kind !== "document"
      ? {
        background: {
          service_worker: "nested/worker.js",
          ...(kind === "module" ? { type: "module" } : {}),
        },
      }
      : {}),
    ...(native
      ? {
        browser_specific_settings: {
          gecko: {
            id,
            ...(compatible
              ? {
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: originalId,
                },
              }
              : {}),
          },
        },
      }
      : {}),
  };
  /** @type {Record<string,string>} */
  const files = {
    "manifest.json": JSON.stringify(manifest),
    "nested/worker.js":
      (kind === "module"
        ? "export const resourceModuleWitness=true;globalThis.resourceModule=resourceModuleWitness;\n"
        : "") + pathProgram,
    "program.js": pathProgram,
    "control.js": controlProgram,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "nested/control.html":
      '<!doctype html><script src="../program.js"></script><script src="../control.js"></script>',
  };
  for (
    const [prefix, label, width, border] of [["", "root", 3, 13], [
      "nested/",
      "nested",
      7,
      19,
    ]]
  ) {
    files[prefix + "scripts/registered.js"] =
      'document.documentElement.setAttribute("data-registered-root",' +
      JSON.stringify(label) + ");";
    files[prefix + "scripts/updated.js"] =
      'document.documentElement.setAttribute("data-registered-root",' +
      JSON.stringify(label + "-updated") + ");";
    files[prefix + "scripts/direct.js"] =
      'document.documentElement.setAttribute("data-direct-root",' +
      JSON.stringify(label) + ");" + JSON.stringify(label) + ";";
    files[prefix + "styles/registered.css"] = "body{outline:" + width +
      "px solid rgb(12,34,56)!important;}";
    files[prefix + "styles/updated.css"] = "body{outline:" +
      (Number(width) + 1) + "px solid rgb(56,34,12)!important;}";
    files[prefix + "styles/direct.css"] = "body{border-top:" + border +
      "px solid rgb(12,34,56)!important;}";
  }
  return files;
}
/** @param {boolean} compatible @param {boolean} native @param {string} originalId @returns {Record<string,string>} */
export function sidebarFiles(compatible, native, originalId) {
  const id = compatible
    ? originalId + "@chromium-extension"
    : "ordinary-sidebar-" + originalId + "@example.invalid";
  return {
    "manifest.json": JSON.stringify({
      manifest_version: 3,
      name: "Owned sidebar height",
      version: "1",
      permissions: ["storage"],
      devtools_page: "devtools.html",
      ...(native
        ? {
          browser_specific_settings: {
            gecko: {
              id,
              ...(compatible
                ? {
                  floorp_chrome_extension: {
                    schema_version: 1,
                    original_id: originalId,
                  },
                }
                : {}),
            },
          },
        }
        : {}),
    }),
    "devtools.html": '<!doctype html><script src="sidebar.js"></script>',
    "sidebar.js": sidebarProgram,
    "control.html": '<!doctype html><script src="control.js"></script>',
    "control.js": controlProgram,
    "sidebar.html": "<!doctype html><p>Owned sidebar page</p>",
  };
}
