#!/bin/csh
# xcat.csh: a C-shell XMLterm wrapper for the UNIX "cat" command
# Usage: xcat.csh <filename1> <filename2> ...

##set echocmd="/usr/bin/echo"
set echocmd="/bin/echo -e"

foreach file ($*)
   set ext=${file:e}
   set firstchar = `echo $file|cut -c1`

   if ("$firstchar" == "/") then
      set url="file:$file"
   else
      set url="file:$PWD/$file"
   endif

   switch ($ext)
   case "gif":
   case "png":
     # Is this a security risk??? Perhaps display using IFRAME?
     $echocmd "\033{S${LTERM_COOKIE}\007\c"
     cat <<EOF
<IMG SRC='$url'>
EOF
     $echocmd '\000\c'
     breaksw
   default:
     cat $file
   endsw
end
