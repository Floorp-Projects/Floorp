"use strict";

/**
 * Given a map from test names to arrays of results, report perfherder metrics
 * and log full results.
 */
function reportMetrics(journal) {
  let metrics = {};
  let text = "\nResults (ms)\n";

  const names = Object.keys(journal);
  const prefixLen = 1 + Math.max(...names.map(str => str.length));

  for (const name in journal) {
    const med = median(journal[name]);
    text += (name + ":").padEnd(prefixLen, " ") + stringify(journal[name]);
    text += "   median " + med + "\n";
    metrics[name] = med;
  }

  dump(text);
  info("perfMetrics", JSON.stringify(metrics));
}

function median(arr) {
  arr = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(arr.length / 2);

  if (arr.length % 2) {
    return arr[mid];
  }

  return (arr[mid - 1] + arr[mid]) / 2;
}

function stringify(arr) {
  function pad(num) {
    let s = num.toString().padStart(5, " ");
    if (s[0] != " ") {
      s = " " + s;
    }
    return s;
  }

  return arr.reduce((acc, elem) => acc + pad(elem), "");
}
