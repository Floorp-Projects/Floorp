@echo off
if %1==baseline goto baseline

:verify
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\viewer_tests\verify\ -rd s:\mozilla\layout\html\tests\table\viewer_tests -f s:\mozilla\layout\html\tests\table\viewer_tests\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\verify\ -rd s:\mozilla\layout\html\tests\table\bugs -f s:\mozilla\layout\html\tests\table\bugs\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\marvin\verify\ -rd s:\mozilla\layout\html\tests\table\marvin -f s:\mozilla\layout\html\tests\table\marvin\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\other\verify\ -rd s:\mozilla\layout\html\tests\table\other -f s:\mozilla\layout\html\tests\table\other\file_list.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\viewer_tests\ -f s:\mozilla\layout\html\tests\table\viewer_tests\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\bugs\ -f s:\mozilla\layout\html\tests\table\bugs\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\marvin\ -f s:\mozilla\layout\html\tests\table\marvin\file_list.txt
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\table\other\ -f s:\mozilla\layout\html\tests\table\other\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) (viewer_tests bugs marvin all)

:done
