eval("\
  x = evalcx('split');\
  evalcx(\"for(e in <x/>){}\" ,x)\
")
