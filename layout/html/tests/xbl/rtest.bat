@echo off
if "%1"=="baseline" goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\xbl\verify\ -rd s:\mozilla\layout\html\tests\xbl -f s:\mozilla\layout\html\tests\xbl\file_list.txt
goto done

:baseline
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\xbl\ -f s:\mozilla\layout\html\tests\xbl\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done

