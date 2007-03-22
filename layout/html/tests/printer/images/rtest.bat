@echo off
if %1==baseline goto baseline
if %1==verify goto verify
if %1==printall goto printall

goto done

:printall
if not exist verify mkdir verify
echo "PRINTING TESTS"
s:\mozilla\dist\bin\viewer -Prt 2  -o s:\mozilla\layout\html\tests\printer\images\verify\  -f s:\mozilla\layout\html\tests\printer\images\file_list.txt -d 5
goto done

:verify
if not exist verify mkdir verify
s:\mozilla\dist\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\printer\images\verify\ -rd s:\mozilla\layout\html\tests\printer\images -f s:\mozilla\layout\html\tests\printer\images\file_list.txt -d 5
goto done

:baseline
s:\mozilla\dist\bin\viewer -Prt 1 -o s:\mozilla\layout\html\tests\printer\images\ -f s:\mozilla\layout\html\tests\printer\images\file_list.txt -d 5
goto done

:error
echo syntax: rtest (baseline verify) 

:done
