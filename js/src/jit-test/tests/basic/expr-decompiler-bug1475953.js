Object.defineProperty(this, "fuzzutils", { value:{} });
setModuleResolveHook(function(module, specifier) {});
try { evaluate(`
  var f = 396684;
  var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";
  var result = new Function(src)();
`);
} catch (exc) {}
try {
  evalInWorker(`
    function lfEvalInCache(lfCode, lfIncremental = false, lfRunOnce = false) {
      ctx = Object.create(ctx, {});
    }
    try { evaluate(\`
      var f = 396684;
      var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";
      var result = new Function(src)();
      \`); } catch(exc) {}
  `);
  evalInWorker(`
    Object.defineProperty(this, "fuzzutils", { value:{} });
    try { evaluate(\`
    var f = 396684;
    var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";
    var result = new Function(src)();
    \`); } catch(exc) {}
  `);
} catch (exc) {}
try { evalInWorker(`
  try { evaluate(\`
    var f = 396684;
    var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";
    var result = new Function(src)();
  \`); } catch(exc) {}
`);
} catch (exc) {}
