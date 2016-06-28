function ok(result, desc) {
  window.parent.postMessage({ok: result, desc: desc}, "*");
}
ok(false, "documents sandboxed with allow-scripts should NOT be able to run <script src=...>");
