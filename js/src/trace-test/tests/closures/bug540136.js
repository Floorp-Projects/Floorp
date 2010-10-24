// |trace-test| error: TypeError

(eval("\
 (function () {\
   for (var[x] = function(){} in \
     (function m(a) {\
       if (a < 1) {\
         x;\
         return\
       }\
       return m(a - 1) + m(a - 2)\
     })(7)\
     (eval(\"\"))\
   )\
   ([])\
 })\
"))()
