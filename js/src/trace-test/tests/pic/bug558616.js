(function() {
  for each(let d in [{}, {}, 0]) {
    for each(e in [0, 0, 0, 0, 0, 0, 0, 0, 0]) {
      d.__defineSetter__("", function() {})
    }
  }
})()

// don't assert
