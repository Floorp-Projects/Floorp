# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

$Env:VIRTUAL_ENV = (gl);
$Env:CUDDLEFISH_ROOT = $Env:VIRTUAL_ENV;

# http://stackoverflow.com/questions/5648931/powershell-test-if-registry-value-exists/5652674#5652674
Function Test-RegistryValue {
    param(
        [Alias("PSPath")]
        [Parameter(Position = 0, Mandatory = $true, ValueFromPipeline = $true, ValueFromPipelineByPropertyName = $true)]
        [String]$Path
        ,
        [Parameter(Position = 1, Mandatory = $true)]
        [String]$Name
        ,
        [Switch]$PassThru
    ) 

    process {
        if (Test-Path $Path) {
            $Key = Get-Item -LiteralPath $Path
            if ($Key.GetValue($Name, $null) -ne $null) {
                if ($PassThru) {
                    Get-ItemProperty $Path $Name
                } else {
                    $true
                }
            } else {
                $false
            }
        } else {
            $false
        }
    }
}

$WINCURVERKEY = 'HKLM:SOFTWARE\Microsoft\Windows\CurrentVersion';
$WIN64 = (Test-RegistryValue $WINCURVERKEY 'ProgramFilesDir (x86)');

if($WIN64) { 
    $PYTHONKEY='HKLM:SOFTWARE\Wow6432Node\Python\PythonCore';
}
else {
    $PYTHONKEY='HKLM:SOFTWARE\Python\PythonCore';
}

$Env:PYTHONVERSION = '';
$Env:PYTHONINSTALL = '';

foreach ($version in @('2.6', '2.5', '2.4')) {
    if (Test-RegistryValue "$PYTHONKEY\$version\InstallPath" '(default)') { 
        $Env:PYTHONVERSION = $version;
    }
}

if ($Env:PYTHONVERSION) { 
    $Env:PYTHONINSTALL = (Get-Item "$PYTHONKEY\$version\InstallPath)").'(default)';
}

if ($Env:PYTHONINSTALL) { 
    $Env:Path += ";$Env:PYTHONINSTALL";
}

if (Test-Path Env:_OLD_PYTHONPATH) { 
    $Env:PYTHONPATH = $Env:_OLD_PYTHONPATH;
}
else {
    $Env:PYTHONPATH = '';
}

$Env:_OLD_PYTHONPATH=$Env:PYTHONPATH;
$Env:PYTHONPATH= "$Env:VIRTUAL_ENV\python-lib;$Env:PYTHONPATH";

if (Test-Path Function:_OLD_VIRTUAL_PROMPT) {
    Set-Content Function:Prompt (Get-Content Function:_OLD_VIRTUAL_PROMPT);
}
else {
    function global:_OLD_VIRTUAL_PROMPT {}
}

Set-Content Function:_OLD_VIRTUAL_PROMPT (Get-Content Function:Prompt);

function global:prompt { "($Env:VIRTUAL_ENV) $(_OLD_VIRTUAL_PROMPT)"; };

if (Test-Path Env:_OLD_VIRTUAL_PATH) {
    $Env:PATH = $Env:_OLD_VIRTUAL_PATH;
}
else {
    $Env:_OLD_VIRTUAL_PATH = $Env:PATH;
}

$Env:Path="$Env:VIRTUAL_ENV\bin;$Env:Path"

[System.Console]::WriteLine("Note: this PowerShell SDK activation script is experimental.")

python -c "from jetpack_sdk_env import welcome; welcome()"

