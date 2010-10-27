(function () {
    try {
        eval("\
          for each(let d in[0,0,0,0,0,0,0,0]) {\
            for(let b in[0,0]) {}\
          }\
        ")
    } catch (e) {}
})()

