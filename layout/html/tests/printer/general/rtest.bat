@echo off
if %1==baseline goto baseline

:verify
if not exist verify mkdir verify
s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\printer\general\verify\ -rd s:\mozilla\layout\html\tests\printer\general -f s:\mozilla\layout\html\tests\printer\general\file_list.txt
goto done

:baseline
s:\mozilla\dist\win32_d.obj\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\printer\general\ -f s:\mozilla\layout\html\tests\printer\general\file_list.txt
goto done

:error
echo syntax: rtest (baseline verify) 

:done
