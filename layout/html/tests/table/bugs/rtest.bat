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
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list1.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list2.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list3.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list4.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list5.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list6.txt
REM s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -B 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list_printing.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list1.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list2.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list3.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list4.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list5.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list6.txt
REM s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list_printing.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done
