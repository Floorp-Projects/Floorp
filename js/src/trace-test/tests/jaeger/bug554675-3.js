(function() {
  try { (function() {
      for each(let x in [0, /x/, 0, {}]) {
        if (x < x) {}
      }
    })()
  } catch(e) {}
})()

/* Don't assert. */

