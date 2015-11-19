@ECHO OFF

REM read config file
setlocal ENABLEDELAYEDEXPANSION
set loop=0
for /F "tokens=*" %%A in (.config) do (
    SET /A loop=!loop! + 1
    set %%A
)

set DEVICE_FOUND=0

REM nexus has device instead of product name
IF [%PRODUCT_NAME%]==[] (
 set PRODUCT_NAME=%DEVICE%
)

REM if nexus 4 assume you are in fastboot mode, can't seem to find drivers 
IF [%DEVICE%]==[mako] (
call :flash
)

REM push device from adb to fastboot mode
win_adb kill-server
win_adb devices
win_adb get-state > devicestate.txt
set /p DEVICE_STATE= < devicestate.txt

IF NOT "%DEVICE_STATE%"=="device" (
   ECHO Please check :
   ECHO 1. to make sure that only one device is connected to the computer
   ECHO 2. the device is turned on with the screen showing
   ECHO 3. the device is set to debugging via USB : ADB Only or ADB and Devtools
   ECHO 4. the device drivers are installed on the computer.
   Del devicestate.txt
   PAUSE
   EXIT /b
)

Del devicestate.txt
win_adb reboot bootloader

TIMEOUT 5

:flash
win_fastboot devices 2> fastboot_state.txt
set /p FASTBOOT_STATE= < fastboot_state.txt

IF NOT [%FASTBOOT_STATE%]==[] (
   ECHO Please check :
   ECHO 1. to make sure that only one device is connected to the computer
   ECHO 2. the device is turned on with an indication that the device is in fastboot mode
   ECHO 3. the fastboot drivers are installed on the computer.
   Del fastboot_state.txt
   PAUSE
   EXIT /b
)

Del fastboot_state.txt

ECHO "Flashing build. If nothing mentions that it flashed anything and it looks stuck, make sure you have the drivers installed."
win_fastboot flash boot out/target/product/%PRODUCT_NAME%/boot.img
win_fastboot flash system out/target/product/%PRODUCT_NAME%/system.img
win_fastboot flash persist out/target/product/%PRODUCT_NAME%/persist.img
win_fastboot flash recovery out/target/product/%PRODUCT_NAME%/recovery.img
win_fastboot flash cache out/target/product/%PRODUCT_NAME%/cache.img
win_fastboot flash userdata out/target/product/%PRODUCT_NAME%/userdata.img

ECHO "Done..."

win_fastboot reboot
echo "Just close the windows as you wish."
TIMEOUT 5
