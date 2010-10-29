for (j = 0; j < 1; j++) {
 var f = eval("\
   (function() {\
     for (var a = 0; a < 8; ++a) {\
       if (a % 3 == 2) {\
         eval(\"\
           for(b in[0,0,0,0]) {\
             print()\
           }\
         \")\
       }\
       gc()\
     }\
   })\
 ");
 f()
}
