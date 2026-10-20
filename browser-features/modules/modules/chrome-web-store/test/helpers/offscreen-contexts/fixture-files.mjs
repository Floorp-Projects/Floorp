// SPDX-License-Identifier: MPL-2.0
import { controlProgram } from "../resource-paths/programs.mjs";
export const publicProgram =
  "// Same authored public program in Chrome and native document/classic/module.\nconst api=globalThis.browser??chrome;\nglobalThis.runResourceOracle=async message=>{\n if(message.method==='cleanup'){\n  await api.offscreen.closeDocument();\n  const immediateRemaining=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});\n  let remaining=immediateRemaining;\n  const start=Date.now(),end=start+10000;\n  while(remaining.length&&Date.now()<end){\n   await new Promise(resolve=>setTimeout(resolve,25));\n   remaining=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});\n  }\n  return {immediateRemaining,remaining,elapsed:Date.now()-start};\n }\n if(message.method!=='probe')throw Error('Unknown owned offscreen command');\n const requestedURL=api.runtime.getURL('offscreen.html');\n await api.offscreen.createDocument({url:requestedURL,reasons:['DOM_PARSER'],justification:'Parse an authored owned document for context identity regression'});\n let identity;\n const end=Date.now()+10000;\n while(Date.now()<end){\n  try{identity=await api.runtime.sendMessage({ownedOffscreenIdentity:true,nonce:message.nonce});if(identity?.nonce===message.nonce)break;}catch{}\n  await new Promise(resolve=>setTimeout(resolve,25));\n }\n if(identity?.nonce!==message.nonce)throw Error('Actual offscreen document identity deadline');\n const unfiltered=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT']});\n const filtered=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentUrls:[requestedURL]});\n const byId=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],contextIds:unfiltered.map(value=>value.contextId)});\n const byDocumentId=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentIds:unfiltered.map(value=>value.documentId)});\n const byReturnedOrigin=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentOrigins:unfiltered.map(value=>value.documentOrigin)});\n const foreign=await api.runtime.getContexts({contextTypes:['OFFSCREEN_DOCUMENT'],documentUrls:['chrome-extension://'+'p'.repeat(32)+'/offscreen.html']});\n return {runtimeId:api.runtime.id,worker:typeof ServiceWorkerGlobalScope!=='undefined'&&self instanceof ServiceWorkerGlobalScope,module:globalThis.offscreenModule===true,requestedURL,urlOrigin:new URL(requestedURL).origin,identity,unfiltered,filtered,byId,byDocumentId,byReturnedOrigin,foreign};\n};\napi.runtime.onMessage.addListener(message=>message.resourceOracle===true?globalThis.runResourceOracle(message):undefined);\n";
/** @param {string} kind @param {string} originalId @param {boolean} native @returns {Record<string,string>} */
export function offscreenFiles(kind, originalId, native) {
  const manifest = {
    manifest_version: 3,
    name: "Owned public offscreen contexts " + kind,
    version: "1",
    permissions: ["offscreen"],
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
            id: originalId + "@chromium-extension",
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: originalId,
            },
          },
        },
      }
      : {}),
  };
  return {
    "manifest.json": JSON.stringify(manifest),
    "program.js": publicProgram,
    "nested/worker.js":
      (kind === "module"
        ? "export const offscreenModuleWitness=true;globalThis.offscreenModule=offscreenModuleWitness;\n"
        : "") + publicProgram,
    "control.html": "<!doctype html>" +
      (kind === "document" ? '<script src="program.js"></script>' : "") +
      '<script src="control.js"></script>',
    "control.js": controlProgram,
    "offscreen.html":
      '<!doctype html><title>Owned real offscreen</title><script src="offscreen.js"></script>',
    "offscreen.js":
      "const api=globalThis.browser??chrome;api.runtime.onMessage.addListener(message=>message.ownedOffscreenIdentity===true?Promise.resolve({runtimeId:api.runtime.id,href:location.href,origin:location.origin,nonce:message.nonce}):undefined);",
  };
}
