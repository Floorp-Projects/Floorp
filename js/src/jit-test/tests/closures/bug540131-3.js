(function() {
  (eval("\
    (function() {\
      let(e = eval(\"\
        for(z=0;z<5;z++){}\
      \"))\
      (function(){\
        x = e\
      })()\
    })\
  "))()
})();
print(x)
