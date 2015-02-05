function ok(result, desc) {
  window.parent.postMessage({ok: result, desc: desc}, "*");
}
ok(true, "documents sandboxed with allow-scripts should be able to run <script src=...>");
