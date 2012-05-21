REM This Source Code Form is subject to the terms of the Mozilla Public
REM License, v. 2.0. If a copy of the MPL was not distributed with this
REM file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
