#!/bin/csh
# xls.csh: a C-shell XMLterm wrapper for the UNIX "ls" command
# Usage: xls.csh [-i] [-x]

set files=(`/bin/ls -d $cwd/*`)
set ncols=4

##set echocmd="/usr/bin/echo"
set echocmd="/bin/echo -e"

set iconic=0
set create=0

set options=""
foreach arg ($*)
   switch ($arg)
   case "-i":
      set iconic=1
      set options=($options $arg)
      breaksw
   case "-c":
      set create=1
      set options=($options $arg)
      breaksw
   endsw
end

$echocmd "\033{S${LTERM_COOKIE}\007\c"
$echocmd '<TABLE FRAME=none BORDER=0>'
$echocmd "<COLGROUP COLSPAN=$ncols WIDTH=1*>"

set rowimg=""
set rowtxt=""
set nfile=0
foreach file ($files)
   if (-d $file) then       #directory
      set filetype="directory"
      set sendtxt="cd $file; xls $options"
      set sendimg="file:/usr/share/pixmaps/mc/i-directory.png"
#      set sendimg="chrome://xmlterm/skin/default/images/ficon3.gif"
   else if (-x $file) then  #executable
      set filetype="executable"
      set sendtxt="$file"
      set sendimg="file:/usr/share/pixmaps/mc/i-executable.png"
   else                     #plain file
      set filetype="plainfile"
      set sendtxt=""
      set sendimg="file:/usr/share/pixmaps/mc/i-regular.png"
   endif

   set tail=${file:t}

   if ($create) then
      set cmd="createln"
   else
      set cmd="sendln"
   endif
   set clickcmd="onClick="'"'"return ClickXMLTerm('$cmd',-1,'$sendtxt')"'"'

   set rowimg="${rowimg}<TD><IMG SRC='$sendimg' $clickcmd>"
   set rowtxt="${rowtxt}<TD><SPAN CLASS='$filetype' $clickcmd>"
   set rowtxt="${rowtxt}$tail<SPAN/>"
@  nfile++

   if (($nfile % $ncols) == 0) then
      if ($iconic) $echocmd "<TR>$rowimg"
      $echocmd "<TR>$rowtxt"
      set rowimg=""
      set rowtxt=""
   endif

end

if ("$rowtxt" != "") then
   if ($iconic) $echocmd "<TR>$rowimg"
   $echocmd "<TR>$rowtxt"
endif

$echocmd '</TABLE>'
$echocmd '\000\c'

