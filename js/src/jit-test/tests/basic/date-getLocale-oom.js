oomTest(function () {
  new Date(NaN).toString();
}, {keepFailing: true});
