import cfg from '../diplomat.config.js';
import {readString} from './diplomat-runtime.js'

let wasm;

const imports = {
  env: {
    diplomat_console_debug_js(ptr, len) {
      console.debug(readString(wasm, ptr, len));
    },
    diplomat_console_error_js(ptr, len) {
      console.error(readString(wasm, ptr, len));
    },
    diplomat_console_info_js(ptr, len) {
      console.info(readString(wasm, ptr, len));
    },
    diplomat_console_log_js(ptr, len) {
      console.log(readString(wasm, ptr, len));
    },
    diplomat_console_warn_js(ptr, len) {
      console.warn(readString(wasm, ptr, len));
    },
    diplomat_throw_error_js(ptr, len) {
      throw new Error(readString(wasm, ptr, len));
    }
  }
}

if (typeof fetch === 'undefined') { // Node
  const fs = await import("fs");
  const wasmFile = new Uint8Array(fs.readFileSync(cfg['wasm_path']));
  const loadedWasm = await WebAssembly.instantiate(wasmFile, imports);
  wasm = loadedWasm.instance.exports;
} else { // Browser
  const loadedWasm = await WebAssembly.instantiateStreaming(fetch(cfg['wasm_path']), imports);
  wasm = loadedWasm.instance.exports;
}

wasm.diplomat_init();
if (cfg['init'] !== undefined) {
  cfg['init'](wasm);
}

export default wasm;
