REM Table regression tests.
REM
REM How to run:
REM 1. Before you make changes, run:
REM    rtest baseline
REM 2. Make your changes and rebuild
REM 3. rtest verify >outfilename
REM
@echo off
if %1==baseline goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -B 1 -o s:\mozilla\layout\html\tests\table\printing\verify\ -rd s:\mozilla\layout\html\tests\table\printing -f s:\mozilla\layout\html\tests\table\printing\file_list.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\table\printing\  -f s:\mozilla\layout\html\tests\table\printing\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done
