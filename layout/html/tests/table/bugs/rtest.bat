@echo off
if %1==baseline goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list.txt
REM file_list2 and file_list3 were added to workaround a javascript bug 
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list2.txt
REM s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list3.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list.txt
REM file_list2 and file_list3 were added to workaround a javascript bug 
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list2.txt
REM s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list3.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done
