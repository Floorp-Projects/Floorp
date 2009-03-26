/* Invoke unit tests on OS/2 */
PARSE ARG dist prog parm
dist=forwardtoback(dist);
prog=forwardtoback(prog);
'set BEGINLIBPATH='dist'\bin;%BEGINLIBPATH%'
'set LIBPATHSTRICT=T'
prog parm
exit

forwardtoback: procedure
  arg pathname
  parse var pathname pathname'/'rest
  do while (rest <> "")
    pathname = pathname'\'rest
    parse var pathname pathname'/'rest
  end
  return pathname
