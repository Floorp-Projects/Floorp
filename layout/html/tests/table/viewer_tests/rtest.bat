@echo off
if %1==baseline goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer  -B 1 -o s:\mozilla\layout\html\tests\table\viewer_tests\verify\ -rd s:\mozilla\layout\html\tests\table\viewer_tests -f s:\mozilla\layout\html\tests\table\viewer_tests\file_list.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\viewer_tests\ -f s:\mozilla\layout\html\tests\table\viewer_tests\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done

