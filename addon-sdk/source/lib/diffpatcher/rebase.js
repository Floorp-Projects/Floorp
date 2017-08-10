"use strict";

var nil = {}
var owns = ({}).hasOwnProperty

function rebase(result, parent, delta) {
  var key, current, previous, update
  for (key in parent) {
    if (owns.call(parent, key)) {
      previous = parent[key]
      update = owns.call(delta, key) ? delta[key] : nil
      if (previous === null) continue
      else if (previous === void(0)) continue
      else if (update === null) continue
      else if (update === void(0)) continue
      else result[key] = previous
    }
  }
  for (key in delta) {
    if (owns.call(delta, key)) {
      update = delta[key]
      current = owns.call(result, key) ? result[key] : nil
      if (current === update) continue
      else if (update === null) continue
      else if (update === void(0)) continue
      else if (current === nil) result[key] = update
      else if (typeof(update) !== "object") result[key] = update
      else if (typeof(current) !== "object") result[key] = update
      else result[key]= rebase({}, current, update)
    }
  }

  return result
}

module.exports = rebase
