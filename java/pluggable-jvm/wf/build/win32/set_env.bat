@REM 
@REM The contents of this file are subject to the Mozilla Public
@REM License Version 1.1 (the "License"); you may not use this file
@REM except in compliance with the License. You may obtain a copy of
@REM the License at http://www.mozilla.org/MPL/
@REM 
@REM Software distributed under the License is distributed on an "AS
@REM IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
@REM implied. See the License for the specific language governing
@REM rights and limitations under the License.
@REM  
@REM The Original Code is The Waterfall Java Plugin Module
@REM  
@REM The Initial Developer of the Original Code is Sun Microsystems Inc
@REM Portions created by Sun Microsystems Inc are Copyright (C) 2001
@REM All Rights Reserved.
@REM 
@REM $Id: set_env.bat,v 1.2 2001/07/12 19:57:43 edburns%acm.org Exp $
@REM 
@REM Contributor(s):
@REM 
@REM     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
@REM 

@echo off
echo Setting some variables for WF....
set WFJDKHOME=c:\jdk1.3
set HOSTINCPATH=d:\inn\libs\include
set HOSTLIBPATH=d:\inn\libs\lib
set PATH=%HOSTLIBPATH%;%PATH%
