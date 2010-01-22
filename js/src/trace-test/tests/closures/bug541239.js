function m() {
 var d = 73;

 return (eval("\n\
   function() {\n\
     return function() {\n\
       yield ((function() {\n\
         print(d);\n\
         return d\n\
       })())\n\
     } ();\n\
   }\n\
 "))();
}

m().next();
