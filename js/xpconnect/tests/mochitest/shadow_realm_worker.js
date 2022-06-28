
var sr = new ShadowRealm();

self.onmessage = async function (e) {
  try {
    // Test evaluate
    if (e.data === 'evaluate') {
      sr.evaluate("var s = 'PASS set string in realm';")
      var res = sr.evaluate('s');
      postMessage(res);
      return;
    }

    // If Import works in a worker, then it ought to work in a shadow realm
    //
    // See  https://bugzilla.mozilla.org/show_bug.cgi?id=1247687 and
    //      https://bugzilla.mozilla.org/show_bug.cgi?id=1772162
    if (e.data == 'import') {
      var import_worked = false;
      var importValue_worked = false;
      var importNested_worked = false;
      try {
        var module = await import("./shadow_realm_module.js");
        if (module.x != 1) {
          throw "mismatch";
        }
        import_worked = true;
      } catch (e) { }

      try {
        await sr.importValue("./shadow_realm_module.js", 'x').then((x) => {
          if (x == 1) { importValue_worked = true; }
        });
      } catch (e) { }

      try {
        sr.evaluate(`
        var imported = false;
        import("./shadow_realm_module.js").then((module) => {
          if (module.x == 1) {
            imported = true;
          }
        });
        true;
        `);

        importNested_worked = sr.evaluate("imported");
      } catch (e) {

      }

      if (importValue_worked == import_worked && importValue_worked == importNested_worked) {
        postMessage(`PASS: importValue, import, and nested import all ${importValue_worked ? "worked" : "failed"} `);
        return;
      }

      postMessage(`FAIL: importValue ${importValue_worked}, import ${import_worked}, importNested ${importNested_worked}`);
      return;
    }


    // Reply back with finish
    if (e.data == 'finish') {
      postMessage("finish");
      return;
    }
  } catch (e) {
    postMessage("FAIL: " + e.message);
  }
  postMessage('Unknown message type.');
}
