@echo off
if "%1"=="baseline" goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\block\bugs\verify\ -rd s:\mozilla\layout\html\tests\block\bugs -f s:\mozilla\layout\html\tests\block\bugs\file_list.txt
REM some files require asynch loading, so they need a short delay to give good results
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\block\bugs\verify\ -rd s:\mozilla\layout\html\tests\block\bugs -f s:\mozilla\layout\html\tests\block\bugs\file_list_slow.txt
goto done

:baseline
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\block\bugs\ -f s:\mozilla\layout\html\tests\block\bugs\file_list.txt
REM some files require asynch loading, so they need a short delay to give good results
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\block\bugs\ -f s:\mozilla\layout\html\tests\block\bugs\file_list_slow.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done

