@echo off
REM  Put it where we all can get it.
REM %1 = release 'or' debug      %2 = builddate

REM  Make the Main repository Folder using the BuildID var
P:
md \client\cck\new\win\5.0\domestic\"%1"\"%2"

  REM  Make the folder for the INI's then copy/move all of them.
  md \client\cck\new\win\5.0\domestic\"%1"\"%2"\iniFiles
  D:
  cd\builds\mozilla\cck\cckwiz\inifiles
  copy *.ini P:\client\cck\new\win\5.0\domestic\"%1"\"%2"\iniFiles
    REM  Copy the wizardmachine.exe to sweetlou
    D:
    cd\builds\mozilla\cck\driver\%1
    copy *.exe P:\client\cck\new\win\5.0\domestic\%1\%2
   
