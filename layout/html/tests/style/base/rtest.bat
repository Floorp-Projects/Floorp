@echo off
if "%1"=="baseline" goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\style\base\verify\ -rd s:\mozilla\layout\html\tests\style\base -f s:\mozilla\layout\html\tests\style\base\file_list1.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -d 1 -o s:\mozilla\layout\html\tests\style\base\ -f s:\mozilla\layout\html\tests\style\base\file_list1.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done
