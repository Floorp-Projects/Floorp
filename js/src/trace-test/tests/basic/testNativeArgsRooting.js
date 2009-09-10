if ('gczeal' in this)
(function () {
   (eval("\
       (function () {\
           for (var y = 0; y < 16; ++y) {\
               if (y % 3 == 2) {\
                   gczeal(1);\
               } else {\
                   print(0 / 0);\
               }\
           }\
       });\
   "))()
})();
