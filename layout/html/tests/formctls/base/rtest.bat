@echo off
if %1==baseline goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\bin\viewer -B 1 -o s:\mozilla\layout\html\tests\formctls\base\verify\ -rd s:\mozilla\layout\html\tests\formctls\base -f s:\mozilla\layout\html\tests\formctls\base\file_list.txt
goto done

:baseline
s:\mozilla\dist\bin\viewer -o s:\mozilla\layout\html\tests\formctls\base\ -f s:\mozilla\layout\html\tests\formctls\base\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 
:done
