#!/usr/bin/env python3
r"""Generate an executable NSIS regression from an actual Runtime checkout.

Run with Python, --runtime <locked checkout> --output <temporary directory>.
Compile each generated .nsi using makensis, then run the .exe on Windows.
Only a unique HKCU Software\FloorpStabilityQA key is written and removed.
This exercises the actual SetUninstallKeys and registry-writing macros, not
the full installer. The key locator and version-migration hook are fixtures;
no installed browser, shortcuts, HKLM or normal uninstall entry is changed.
Baseline Floorp branding fails; patched official/daylight and fallback pass.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import uuid


def macro(source: str, name: str) -> str:
    found = re.search(r"^!macro " + re.escape(name) + r"(?:\s[^\n]*)?\n.*?^!macroend\s*$",
                      source, re.MULTILINE | re.DOTALL)
    if not found:
        raise ValueError(f"Missing Runtime macro {name}")
    return found.group(0) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    shared = (args.runtime / "browser/installer/windows/nsis/shared.nsh").read_text(encoding="utf-8")
    common = (args.runtime / "toolkit/mozapps/installer/windows/nsis/common.nsh").read_text(encoding="utf-8")
    actual = macro(shared, "SetUninstallKeys")
    for name in ["WriteRegStr2", "WriteRegStr2Call", "WriteRegDWORD2", "WriteRegDWORD2Call", "GetLongPath", "GetLongPathCall"]:
        actual += macro(common, name)
    (args.output / "actual-runtime-macros.nsh").write_text(actual, encoding="utf-8")
    for brand in ["floorp-official", "floorp-daylight", "fallback"]:
        source_brand = "floorp-official" if brand == "fallback" else brand
        branding = (args.runtime / f".github/assets/branding/{source_brand}/branding.nsi").read_text(encoding="utf-8")
        if brand == "fallback":
            branding = re.sub(r'^!define BrandUninstallPublisher[^\n]*\n', '', branding, flags=re.MULTILINE)
        (args.output / f"{brand}-branding.nsi").write_text(branding, encoding="utf-8")
        expected = "Mozilla" if brand == "fallback" else "Floorp Projects"
        key = "Software\\FloorpStabilityQA\\KAN88-" + uuid.uuid4().hex
        harness = r'''Unicode true
RequestExecutionLevel user
SilentInstall silent
Name "Floorp publisher isolated regression"
OutFile "@BRAND@.exe"
!include LogicLib.nsh
!include FileFunc.nsh
!include WordFunc.nsh
!include WinVer.nsh
!include "@BRAND@-branding.nsi"
!define _MOZFUNC_UN ""
!define _MOZFUNC_VERBOSE 2
!define NO_LOG
!define UpdateChannel "release"
!define AppVersion "12.21.1"
!define ARCH "x64"
!define AB_CD "en-US"
!define FileMainEXE "floorp.exe"
!define IDI_APPICON_ZERO_BASED 0
Var RegHive
Var ExistingMachineKey
Var Failures
!macro RegistryWriteVersion
  Pop $0
!macroend
!include actual-runtime-macros.nsh
!insertmacro WriteRegStr2
!insertmacro WriteRegDWORD2
!insertmacro GetLongPath
Function getUninstallKey
  Pop $0
  Push "@KEY@"
FunctionEnd
Function findUninstallKey
  Push $ExistingMachineKey
FunctionEnd
!macro Check CASE EXPECTED
  ReadRegStr $1 HKCU "@KEY@" "Publisher"
  WriteINIStr "$EXEDIR\@BRAND@-result.ini" "${CASE}" "Publisher" "$1"
  ${If} $1 != "${EXPECTED}"
    IntOp $Failures $Failures + 1
  ${EndIf}
!macroend
Section
  SetRegView 64
  SetShellVarContext current
  StrCpy $RegHive "HKCU"
  StrCpy $ExistingMachineKey ""
  StrCpy $INSTDIR "$EXEDIR"
  StrCpy $Failures 0
  !insertmacro SetUninstallKeys
  !insertmacro Check fresh "@EXPECTED@"
  ReadRegStr $1 HKCU "@KEY@" "DisplayName"
  ${If} $1 != "${BrandFullNameInternal} (x64 en-US)"
    IntOp $Failures $Failures + 1
  ${EndIf}
  WriteRegStr HKCU "@KEY@" "Publisher" "Mozilla"
  !insertmacro SetUninstallKeys
  !insertmacro Check existing "@EXPECTED@"
  ; Existing machine install found by the real helper must suppress user entry.
  StrCpy $ExistingMachineKey "existing-machine-install-fixture"
  WriteRegStr HKCU "@KEY@" "Publisher" "untouched-control"
  !insertmacro SetUninstallKeys
  !insertmacro Check machine_install_guard "untouched-control"
  DeleteRegKey HKCU "@KEY@"
  WriteINIStr "$EXEDIR\@BRAND@-result.ini" "summary" "failures" "$Failures"
  SetErrorLevel $Failures
SectionEnd
'''.replace("@BRAND@", brand).replace("@KEY@", key).replace("@EXPECTED@", expected)
        (args.output / f"{brand}.nsi").write_text(harness, encoding="utf-8")
    (args.output / "source-evidence.json").write_text(json.dumps({
        "shared_sha256": hashlib.sha256(shared.encode()).hexdigest(),
        "common_sha256": hashlib.sha256(common.encode()).hexdigest(),
        "scope": "Actual NSIS macro; per-user registry only, key locators are isolated fixtures; not full installation or MAR post-update",
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
