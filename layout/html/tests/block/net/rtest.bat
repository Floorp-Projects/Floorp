@echo off
if "%1"=="baseline" goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\block\net\verify\ -rd s:\mozilla\layout\html\tests\block\net -f s:\mozilla\layout\html\tests\block\net\file_list.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -o s:\mozilla\layout\html\tests\block\net\ -f s:\mozilla\layout\html\tests\block\net\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done

